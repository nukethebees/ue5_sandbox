#include "player/window.h"
#include <ioj/host/game_host_api.h>
#include <ioj/settings/game_window_mode.h>
#include <ioj/settings/player_settings.h>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_video.h>

#include <expected>
#include <utility>

namespace ioj::player {
auto Window::display() const -> std::expected<WindowDisplaySettings, std::string> {
    WindowDisplaySettings actual;
    if (!SDL_GetWindowSizeInPixels(window_, &actual.resolution.width, &actual.resolution.height)) {
        return std::unexpected{std::string{SDL_GetError()}};
    }
    actual.mode = SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN ? GameWindowMode::Borderless
                                                                      : GameWindowMode::Windowed;
    return actual;
}

auto Window::synchronize(bool activate) -> Result {
    int x{};
    int y{};
    auto actual{display()};
    if (!actual) {
        return std::unexpected{std::move(actual.error())};
    }
    if (!SDL_GetWindowPosition(window_, &x, &y)) {
        return std::unexpected{std::string{SDL_GetError()}};
    }
    if (!IojPlayer_InitializeWindow(
            x, y, actual->resolution.width, actual->resolution.height, activate)) {
        return std::unexpected{"Slate or Unreal viewport differs from the host client area"};
    }
    return {};
}

auto Window::configure(WindowDisplaySettings requested)
    -> std::expected<WindowDisplaySettings, std::string> {
    bool const fullscreen{requested.mode != GameWindowMode::Windowed};
    if (!SDL_SetWindowFullscreen(window_, fullscreen) ||
        (!fullscreen &&
         !SDL_SetWindowSize(window_, requested.resolution.width, requested.resolution.height)) ||
        !SDL_SyncWindow(window_)) {
        return std::unexpected{std::string{SDL_GetError()}};
    }
    auto actual{display()};
    if (actual && !fullscreen && actual->resolution != requested.resolution) {
        return std::unexpected{"SDL did not apply the requested client size"};
    }
    return actual;
}

auto Window::preview(WindowDisplaySettings requested) -> Result {
    auto const flags{SDL_GetWindowFlags(window_)};
    Preview previous{.maximized = (flags & SDL_WINDOW_MAXIMIZED) != 0,
                     .fullscreen = (flags & SDL_WINDOW_FULLSCREEN) != 0};
    if (previous.maximized && (!SDL_RestoreWindow(window_) || !SDL_SyncWindow(window_))) {
        return std::unexpected{std::string{SDL_GetError()}};
    }

    if (!SDL_GetWindowPosition(window_, &previous.floating.x, &previous.floating.y) ||
        !SDL_GetWindowSize(window_, &previous.floating.w, &previous.floating.h)) {
        return std::unexpected{std::string{SDL_GetError()}};
    }
    preview_ = previous;
    auto actual{configure(requested)};
    Result result;
    if (actual) {
        preview_->requested = *actual;
        result = synchronize(false);
    } else {
        result = std::unexpected{std::move(actual.error())};
    }
    if (!result) {
        if (auto rollback{revert_preview()}; !rollback) {
            result.error() += "; rollback failed: " + rollback.error();
        }
    }
    return result;
}

void Window::keep_preview(PlayerSettings& settings) {
    if (preview_) {
        settings.set_running_display(preview_->requested);
        preview_.reset();
    }
}

auto Window::verify_preview() const -> Result {
    if (preview_) {
        auto actual{display()};
        if (!actual) {
            return std::unexpected{std::move(actual.error())};
        }
        if (actual->mode != preview_->requested.mode ||
            actual->resolution != preview_->requested.resolution) {
            return std::unexpected{"The display changed during confirmation"};
        }
    }
    return {};
}

auto Window::revert_preview() -> Result {
    if (!preview_) {
        return {};
    }
    auto const& previous{*preview_};
    if (!SDL_SetWindowFullscreen(window_, previous.fullscreen) ||
        (!previous.fullscreen &&
         (!SDL_SetWindowSize(window_, previous.floating.w, previous.floating.h) ||
          !SDL_SetWindowPosition(window_, previous.floating.x, previous.floating.y))) ||
        (previous.maximized && !SDL_MaximizeWindow(window_)) || !SDL_SyncWindow(window_)) {
        return std::unexpected{std::string{SDL_GetError()}};
    }
    auto result{synchronize(false)};
    if (result) {
        preview_.reset();
    }
    return result;
}
}
