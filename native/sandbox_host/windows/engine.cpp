#include "../engine.h"

#include "../cli_args.h"
#include "library.h"
#include "system.h"
#include "window.h"
#include <ioj/host/game_host.h>
#include <ioj/settings/launch_display.h>

#include <cassert>
#include <expected>
#include <filesystem>
#include <format>
#include <memory>
#include <string>
#include <utility>

namespace ioj::sandbox_host {
struct NativeEngine {
    Library library;
    Window window;
};

namespace {
auto engine_arguments(std::filesystem::path const& directory,
                      LaunchDisplay display,
                      std::filesystem::path const& user_directory) -> std::wstring {
    auto const project{directory.parent_path().parent_path() / L"Sandbox.uproject"};
    auto arguments{
        std::format(L"\"{}\" /SpaceGame/Levels/MainMenu -game -windowed", project.native())};

    if (display == LaunchDisplay::Headless) {
        arguments += L" -RenderOffScreen -nullrhi -nosound -nosplash -unattended";
    } else {
        arguments += L" -UELibraryWindowActivation";
    }

    if (!user_directory.empty()) {
        arguments += std::format(L" -SpaceSaveLocal -UserDir=\"{}\"",
                                 std::filesystem::absolute(user_directory).native());
    }

    return arguments;
}
}

Engine::Engine() = default;

Engine::~Engine() {
    if (native_) {
        shutdown();
    }
}

auto Engine::start(LaunchPaths const& paths, CliArgs const& args, GameHost& host) -> Result {
    assert(!native_);
    auto native{std::make_unique<NativeEngine>()};
    auto& library{native->library};
    if (auto result{library.load(paths.binaries / SANDBOX_GAME_DLL)}; !result) {
        return result;
    }

    auto desktop{desktop_resolution()};
    if (!desktop) {
        return Result{std::unexpect, std::move(desktop.error())};
    }

    auto const instance{GetModuleHandleW(nullptr)};
    auto& window{native->window};
    if (auto result{window.create(instance)}; !result) {
        return result;
    }

    auto const arguments{engine_arguments(paths.binaries, args.display, args.user_directory)};
    host.window = &window;
    host.apply_display = Window::apply;

    HostDisplay saved{};
    if (auto error{library.prepare(
            &host, paths.scripts.c_str(), paths.personal.c_str(), *desktop, &saved)}) {
        return Result{std::unexpect, error};
    }

    if (auto display{initial_display(args, saved)}) {
        if (auto result{window.resize(*display)}; !result) {
            library.detach();
            return result;
        }
    }

    if (auto status{library.init(instance, window.handle(), arguments.c_str())}; status != 0) {
        library.detach();
        return Result{std::unexpect, std::format("UELibrary_InitW failed ({})", status)};
    }

    native_ = std::move(native);
    return {};
}

auto Engine::world_ready() const -> bool {
    return native_->library.ready();
}

auto Engine::show_window() -> Result {
    return native_->window.show(native_->library);
}

auto Engine::launch_mission(std::filesystem::path const& mission) -> bool {
    return native_->library.launch(mission.c_str());
}

auto Engine::exit_requested() const -> bool {
    return native_->window.closed() || native_->library.wants_exit();
}

void Engine::pump_messages() {
    native_->library.pump();
}

auto Engine::synchronize_window() -> Result {
    return native_->window.synchronize_resize();
}

auto Engine::tick() -> Result {
    if (auto status{native_->library.tick()}; status != 0) {
        return Result{std::unexpect, std::format("UELibrary_Tick failed ({})", status)};
    }

    return {};
}

auto Engine::shutdown() -> int {
    assert(native_);
    auto native{std::move(native_)};
    native->window.disconnect();

    auto const status{native->library.shutdown()};
    native->library.detach();
    return status;
}
} // namespace ioj::sandbox_host
