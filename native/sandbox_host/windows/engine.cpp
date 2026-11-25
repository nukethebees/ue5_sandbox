#include "../engine.h"

#include "../cli_args.h"
#include "library.h"
#include "system.h"
#include "window.h"
#include <ioj/host/mission_startup.h>
#include <ioj/settings/launch_display_mode.h>
#include <ioj/settings/window_display_settings.h>
#include <ioj/settings/windows_display.h>

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

Engine::Engine() = default;

Engine::~Engine() {
    if (native_) {
        shutdown();
    }
}

auto Engine::start(LaunchPaths const& paths, CliArgs const& args) -> Result {
    assert(!native_);
    auto native{std::make_unique<NativeEngine>()};
    auto& library{native->library};
    if (auto result{library.load(paths.binaries / SANDBOX_GAME_DLL)}; !result) {
        return result;
    }

    auto& window{native->window};
    if (auto result{window.create()}; !result) {
        return result;
    }
    auto desktop{window.desktop_resolution()};
    if (!desktop) {
        return Result{std::unexpect, std::move(desktop.error())};
    }

    auto const arguments{engine_arguments(paths.binaries, args.display, args.user_directory)};

    WindowDisplaySettings saved{};
    if (auto error{library.prepare(
            args.display, paths.scripts.c_str(), paths.personal.c_str(), *desktop, &saved)}) {
        return Result{std::unexpect, error};
    }

    if (auto display{initial_display(args, saved)}) {
        if (auto result{window.resize(*display)}; !result) {
            library.detach();
            return result;
        }
        auto actual{WindowsDisplay{window.handle()}.inspect()};
        if (!actual) {
            library.detach();
            return Result{std::unexpect, std::move(actual.error())};
        }
        library.initial_display(*actual);
    }

    if (auto status{library.init(GetModuleHandleW(nullptr), window.handle(), arguments.c_str())};
        status != 0) {
        library.detach();
        return Result{std::unexpect, std::format("UELibrary_InitW failed ({})", status)};
    }

    native_ = std::move(native);
    return {};
}

auto Engine::mission_startup() const -> MissionStartup {
    return native_->library.mission_startup();
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
