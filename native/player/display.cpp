#include "window.h"
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
}
