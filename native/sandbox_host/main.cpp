#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <ioj/levels/authoring/game_configuration.h>
#include <ioj/sim/player/embedded_game_host.h>

#include <windows.h>

#include <CLI/CLI.hpp>
#include <shlobj.h>
#include <UnrealAgilitySDKLink.inl>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using MessageHandler = LRESULT (*)(HWND, UINT, WPARAM, LPARAM);
enum class LaunchDisplay { Preferences, Windowed, Headless };
LaunchDisplay launch_display{LaunchDisplay::Preferences};
MessageHandler engine_window_proc{};
bool close_requested{};

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_CLOSE) {
        close_requested = true;
        return 0;
    }
    if (engine_window_proc) {
        return engine_window_proc(window, message, wparam, lparam);
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

template <typename Function>
auto load_export(HMODULE library, char const* name) -> Function {
    auto const address{GetProcAddress(library, name)};
    if (!address) {
        throw std::runtime_error{std::string{"Game DLL is missing export "} + name};
    }
    return reinterpret_cast<Function>(address);
}

auto executable_directory() -> std::filesystem::path {
    std::wstring buffer(32768, L'\0');
    auto const size{GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()))};
    if (size == 0 || size >= buffer.size()) {
        throw std::runtime_error{"Cannot locate SandboxHost.exe"};
    }
    buffer.resize(size);
    return std::filesystem::path{buffer}.parent_path();
}

HWND game_window{};

void apply_display(ioj::GameSettingsState const& state) {
    if (launch_display == LaunchDisplay::Headless) {
        return;
    }
    auto const force_window{launch_display == LaunchDisplay::Windowed};
    auto const windowed{force_window || state.window_mode == ioj::GameWindowMode::Windowed};
    auto const style{windowed ? WS_OVERLAPPEDWINDOW : WS_POPUP};
    RECT bounds{0, 0, state.resolution.width, state.resolution.height};
    if (force_window) {
        bounds.right = std::min(bounds.right, 1280L);
        bounds.bottom = std::min(bounds.bottom, 720L);
    }
    UINT flags{SWP_FRAMECHANGED | SWP_NOZORDER};
    if (windowed) {
        AdjustWindowRectEx(&bounds, style, FALSE, 0);
        flags |= SWP_NOMOVE;
    } else {
        MONITORINFO monitor{sizeof(MONITORINFO)};
        GetMonitorInfoW(MonitorFromWindow(game_window, MONITOR_DEFAULTTONEAREST), &monitor);
        bounds = monitor.rcMonitor;
    }
    SetWindowLongPtrW(
        game_window, GWL_STYLE, style | (IsWindowVisible(game_window) ? WS_VISIBLE : 0));
    SetWindowPos(game_window,
                 nullptr,
                 bounds.left,
                 bounds.top,
                 bounds.right - bounds.left,
                 bounds.bottom - bounds.top,
                 flags);
}

auto preferences_file() -> std::filesystem::path {
    PWSTR local{};
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local))) {
        throw std::runtime_error{"Cannot locate the personal settings directory"};
    }
    std::filesystem::path directory{local};
    CoTaskMemFree(local);
    return directory / L"IsleOfJebbers/Config/user-settings.scm";
}

auto run(std::filesystem::path mission,
         std::filesystem::path const& editor_scripts,
         bool const exit_when_ready) -> int {
    auto const directory{executable_directory()};
    auto const project_directory{directory.parent_path().parent_path()};
    if (!mission.empty()) {
        mission = (project_directory / mission).lexically_normal();
        if (!std::filesystem::is_regular_file(mission)) {
            std::wcerr << L"Mission file does not exist: " << mission << L'\n';
            return 1;
        }
    }
    auto const scripts{editor_scripts.empty() ? project_directory / L"LevelScripts"
                                              : std::filesystem::absolute(editor_scripts)};
    auto const personal{editor_scripts.empty() ? preferences_file()
                                               : scripts / L"Settings/user-settings.scm"};
    auto const settings{ioj::levels::authoring::read_application_settings(scripts)};
    if (!settings) {
        throw std::runtime_error{settings.error()};
    }
    auto const preferences{ioj::levels::authoring::read_user_settings(
        scripts, personal, {GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)})};
    if (!preferences) {
        throw std::runtime_error{preferences.error()};
    }
    std::wcout << L"Preferences: " << personal << L'\n';
    ioj::EmbeddedGameHost host{*settings, {}, personal.c_str(), apply_display};
    host.session.initialise(preferences->applied, preferences->defaults);
    auto const library_path{directory / L"SandboxEmbedded.dll"};
    auto const library{LoadLibraryW(library_path.c_str())};
    if (!library) {
        throw std::system_error{static_cast<int>(GetLastError()),
                                std::system_category(),
                                "Loading staged SandboxEmbedded.dll"};
    }
    auto const init{
        load_export<int (*)(HINSTANCE, HWND, wchar_t const*)>(library, "UELibrary_InitW")};
    auto const tick{load_export<int (*)()>(library, "UELibrary_Tick")};
    auto const shutdown{load_export<int (*)()>(library, "UELibrary_Shutdown")};
    auto const is_ready{load_export<bool (*)()>(library, "SandboxHost_GameReady")};
    auto const wants_exit{load_export<bool (*)()>(library, "SandboxHost_ExitRequested")};
    auto const pump_messages{load_export<void (*)()>(library, "SandboxHost_PumpMessages")};
    auto const initialize_window{
        load_export<void (*)(int, int, int, int)>(library, "SandboxHost_InitializeWindow")};
    auto const attach{load_export<void (*)(ioj::EmbeddedGameHost*)>(library, "SandboxHost_Attach")};
    auto const launch{load_export<bool (*)(wchar_t const*)>(library, "SandboxHost_LaunchMission")};
    if (launch_display != LaunchDisplay::Headless) {
        engine_window_proc = load_export<MessageHandler>(library, "UELibrary_WndProc");
    }

    auto const instance{GetModuleHandleW(nullptr)};
    WNDCLASSW window_class{};
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = L"SandboxHost";
    window_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    if (!RegisterClassW(&window_class)) {
        throw std::runtime_error{"Could not register the Sandbox window"};
    }
    auto const window{CreateWindowExW(0,
                                      window_class.lpszClassName,
                                      L"Sandbox",
                                      WS_OVERLAPPEDWINDOW,
                                      CW_USEDEFAULT,
                                      CW_USEDEFAULT,
                                      1280,
                                      720,
                                      nullptr,
                                      nullptr,
                                      instance,
                                      nullptr)};
    if (!window) {
        throw std::runtime_error{"Could not create the Sandbox window"};
    }
    game_window = window;
    apply_display(host.session.applied_state());
    attach(&host);
    auto const project{project_directory / L"Sandbox.uproject"};
    auto arguments{L"\"" + project.wstring() + L"\" /SpaceGame/Levels/MainMenu -game -windowed"};
    if (launch_display == LaunchDisplay::Headless) {
        arguments += L" -RenderOffScreen -nullrhi -nosound -nosplash -unattended";
    } else {
        arguments += L" -UELibraryWindowActivation";
    }
    if (init(instance, window, arguments.c_str()) != 0 || !is_ready()) {
        throw std::runtime_error{"Sandbox did not initialize a playable world. See the game log."};
    }
    if (launch_display != LaunchDisplay::Headless) {
        POINT origin{};
        RECT client{};
        if (!ClientToScreen(window, &origin) || !GetClientRect(window, &client)) {
            shutdown();
            throw std::runtime_error{"Could not read the Sandbox window geometry"};
        }
        initialize_window(origin.x, origin.y, client.right, client.bottom);
        ShowWindow(window, SW_SHOW);
    }
    if (!mission.empty() && !launch(mission.c_str())) {
        shutdown();
        throw std::runtime_error{"Mission launch failed. See the game log."};
    }
    auto const deadline{std::chrono::steady_clock::now() + std::chrono::minutes{2}};
    while (!close_requested && !wants_exit()) {
        pump_messages();
        if (close_requested || wants_exit()) {
            break;
        }
        if (exit_when_ready) {
            if (mission.empty() || host.mission_startup == ioj::MissionStartup::Ready) {
                std::wcout << L"Ready: " << (mission.empty() ? L"MainMenu" : mission.c_str())
                           << L'\n';
                return shutdown();
            }
            if (host.mission_startup == ioj::MissionStartup::Failed ||
                std::chrono::steady_clock::now() >= deadline) {
                shutdown();
                throw std::runtime_error{
                    "Mission did not reach simulation readiness. See the game log."};
            }
        }
        if (tick() != 0) {
            shutdown();
            throw std::runtime_error{"Unreal tick failed"};
        }
    }
    auto const result{shutdown()};
    return exit_when_ready ? 1 : result;
}
}

int wmain(int argc, wchar_t** argv) {
    CLI::App app{"Sandbox native game host"};
    std::filesystem::path mission;
    std::filesystem::path editor_scripts;
    bool exit_when_ready{};
    bool headless{};
    bool windowed{};
    auto* const headless_option{
        app.add_flag("--headless", headless, "Run without a visible game window or audio")};
    app.add_flag("--windowed", windowed, "Use a resizable window up to 1280 x 720 for this run")
        ->excludes(headless_option);
    app.add_flag("--exit-when-ready",
                 exit_when_ready,
                 "Exit after the menu or requested mission is ready (headless by default)");
    app.add_option(
        "--mission", mission, "S7 mission script (absolute or relative to the staged project)");
    app.add_option("--editor-scripts",
                   editor_scripts,
                   "Use the editor's LevelScripts directory for application settings and personal "
                   "preferences")
        ->check(CLI::ExistingDirectory);
    CLI11_PARSE(app, argc, argv);
    if (windowed) {
        launch_display = LaunchDisplay::Windowed;
    } else if (headless || exit_when_ready) {
        launch_display = LaunchDisplay::Headless;
    }
    try {
        return run(mission, editor_scripts, exit_when_ready);
    } catch (std::exception const& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
