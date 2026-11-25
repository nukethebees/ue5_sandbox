#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
#include "GameHost.h"

#include "GenericPlatform/GenericWindow.h"
#include "Slate/SceneViewport.h"
#include "SpaceGame/system/GameSubsystem.h"
#include "UnrealEngine.h"
#include <ioj/host/game_host_api.h>
#include <ioj/host/mission_startup.h>
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/levels/control_context.h>
#include <ioj/levels/diagnostics.h>
#include <ioj/levels/launch_options.h>
#include <ioj/settings/player_settings.h>
#include <ioj/settings/screen_resolution.h>
#include <ioj/settings/window_display_settings.h>
#include <ioj/settings/windows_display.h>

#include <SandboxCoreEngine/strings.h>

#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/Platform.h"
#include "HAL/PlatformApplicationMisc.h" // IWYU pragma: keep
#include "Kismet/GameplayStatics.h"
#include "Logging/LogMacros.h"
#include "Logging/LogVerbosity.h"
#include "Misc/AssertionMacros.h"
#include "Widgets/SWindow.h"

#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <utility>

namespace ioj {
namespace {
MissionStartup mission_status{MissionStartup::Pending};
std::optional<WindowsDisplay> window_preview;

auto viewport_window() -> HWND {
    auto const viewport{GEngine ? GEngine->GameViewport.Get() : nullptr};
    auto const window{viewport ? viewport->GetWindow() : nullptr};
    auto const native{window ? window->GetNativeWindow() : nullptr};
    return native ? static_cast<HWND>(native->GetOSWindowHandle()) : nullptr;
}

auto synchronize_game_window(HWND hwnd) -> SettingsResult {
    POINT origin{};
    RECT client{};
    if (!ClientToScreen(hwnd, &origin) || !GetClientRect(hwnd, &client) ||
        !SandboxHost_InitializeWindow(origin.x, origin.y, client.right, client.bottom, false)) {
        return SettingsResult{std::unexpect,
                              "Could not synchronize the game viewport with the window"};
    }
    return {};
}
std::optional<PlayerSettings> prepared_settings;
std::string prepare_error;
}
auto SandboxHost_MissionStartup() -> MissionStartup {
    return mission_status;
}
void report_mission_startup(MissionStartup status) {
    mission_status = status;
}
auto inspect_game_window() -> std::expected<WindowDisplaySettings, std::string> {
    return WindowsDisplay{viewport_window()}.inspect();
}
auto preview_game_window(WindowDisplaySettings display) -> SettingsResult {
    auto hwnd{viewport_window()};
    auto& preview{window_preview.emplace(hwnd)};
    auto result{preview.preview_display(display)};
    if (!result) {
        window_preview.reset();
        return result;
    }
    result = synchronize_game_window(hwnd);
    if (!result) {
        if (auto rollback{revert_game_window()}; !rollback) {
            result.error() += "; " + rollback.error();
        }
    }
    return result;
}
auto verify_game_window() -> SettingsResult {
    check(window_preview);
    auto result{window_preview->verify_preview()};
    return result ? synchronize_game_window(viewport_window()) : result;
}
void keep_game_window() {
    window_preview.reset();
}
auto revert_game_window() -> SettingsResult {
    check(window_preview);
    auto result{window_preview->restore()};
    if (result) {
        result = synchronize_game_window(viewport_window());
    }
    if (result) {
        window_preview.reset();
    }
    return result;
}

auto SandboxHost_Prepare(LaunchDisplayMode mode,
                         wchar_t const* scripts,
                         wchar_t const* personal,
                         ScreenResolution desktop,
                         WindowDisplaySettings* saved) -> char const* {
    check(!prepared_settings);
    PlayerSettings settings;
    if (auto result{settings.load(scripts, personal, desktop, mode)}; !result) {
        prepare_error = std::move(result.error());
        return prepare_error.c_str();
    }

    *saved = settings.saved_display();
    prepared_settings.emplace(std::move(settings));
    return nullptr;
}

void SandboxHost_SetInitialDisplay(WindowDisplaySettings actual) {
    check(prepared_settings);
    prepared_settings->set_running_display(actual);
}

auto take_prepared_settings() -> PlayerSettings {
    check(prepared_settings.has_value());
    return std::move(*std::exchange(prepared_settings, std::nullopt));
}

void SandboxHost_Detach() {
    prepared_settings.reset();
    window_preview.reset();
    mission_status = MissionStartup::Pending;
}

auto SandboxHost_GameReady() -> bool {
    auto const* viewport{GEngine ? GEngine->GameViewport.Get() : nullptr};
    auto const* world{viewport ? viewport->GetWorld() : nullptr};
    return world && world->HasBegunPlay();
}

auto SandboxHost_ExitRequested() -> bool {
    return IsEngineExitRequested();
}

void SandboxHost_PumpMessages() {
    FPlatformApplicationMisc::PumpMessages(true);
}

auto SandboxHost_InitializeWindow(int x, int y, int width, int height, bool activate) -> bool {
    auto const window{GEngine->GameViewport->GetWindow().ToSharedRef()};
    auto const native_window{window->GetNativeWindow().ToSharedRef()};
    auto* viewport{GEngine->GameViewport->GetGameViewport()};
    auto& slate{FSlateApplication::Get()};

    GUELibraryOverrideSettings.WindowWidth = width;
    GUELibraryOverrideSettings.WindowHeight = height;
    GSystemResolution.ResX = width;
    GSystemResolution.ResY = height;

    slate.OnMovedWindow(native_window, x, y);
    slate.OnSizeChanged(native_window, width, height);
    if (viewport && viewport->GetSizeXY() != FIntPoint{width, height}) {
        viewport->SetViewportSize(width, height);
        window->SetViewportSizeDrivenByWindow(true);
    }

    if (activate) {
        slate.ProcessApplicationActivationEvent(native_window->IsForegroundWindow());
    }

    auto const measured{window->GetSizeInScreen()};
    return measured.X == width && measured.Y == height && viewport &&
           viewport->GetSizeXY() == FIntPoint{width, height};
}

auto SandboxHost_LaunchMission(wchar_t const* path) -> bool {
    auto* const world{GEngine->GameViewport->GetWorld()};
    auto* const game{UGameSubsystem::get(world->GetGameInstance())};
    check(game);

    auto definition{levels::authoring::DefinitionReader::for_level(path).read_level_file(path)};
    if (!definition) {
        auto const message{levels::format_diagnostics(definition.error())};
        UE_LOG(LogTemp, Error, TEXT("Failed to read mission %s: %s"), path, *to_fstring(message));
        return false;
    }

    mission_status = MissionStartup::Pending;
    levels::LaunchOptions options;
    options.control_context =
        definition->camera() ? levels::ControlContext::Observer : levels::ControlContext::Player;
    game->set_pending_level(std::move(*definition), FString{path}, options);

    UGameplayStatics::OpenLevel(world, TEXT("/SpaceGame/Levels/GameRuntime"));
    return true;
}
}
#endif
