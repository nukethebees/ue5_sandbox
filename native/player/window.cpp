#include "window.h"

#include <ioj/settings/player_settings.h>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_video.h>

#include <algorithm>
#include <memory>

namespace ioj::player {
Window::~Window() {
    if (window_) {
        SDL_DestroyWindow(window_);
    }

    SDL_Quit();
}

auto Window::create() -> Result {
    SDL_SetMainReady();
    // Slate pumps the process message queue and owns keyboard and mouse input.
    SDL_SetHint(SDL_HINT_WINDOWS_ENABLE_MESSAGELOOP, "0");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        return Result{std::unexpect, SDL_GetError()};
    }

    window_ =
        SDL_CreateWindow("Isle of Jebbers",
                         startup_size.width,
                         startup_size.height,
                         SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window_) {
        return Result{std::unexpect, SDL_GetError()};
    }

    return refresh_resolutions();
}

auto Window::desktop_resolution() const -> std::expected<ScreenResolution, std::string> {
    auto const* mode{SDL_GetDesktopDisplayMode(SDL_GetDisplayForWindow(window_))};
    if (!mode) {
        return std::unexpected{std::string{SDL_GetError()}};
    }

    return ScreenResolution{mode->w, mode->h};
}

auto Window::refresh_resolutions() -> Result {
    int count{};
    auto** modes{SDL_GetFullscreenDisplayModes(SDL_GetDisplayForWindow(window_), &count)};
    if (!modes) {
        return std::unexpected{std::string{SDL_GetError()}};
    }

    std::unique_ptr<SDL_DisplayMode* [], decltype(&SDL_free)> const display_modes {
        modes, SDL_free
    };
    supported_resolutions_.clear();
    for (int index{}; index < count; ++index) {
        ScreenResolution const resolution{display_modes[index]->w, display_modes[index]->h};
        if (std::ranges::find(supported_resolutions_, resolution) == supported_resolutions_.end()) {
            supported_resolutions_.push_back(resolution);
        }
    }
    return {};
}

auto Window::show(PlayerSettings& settings) -> Result {
    if (!SDL_ShowWindow(window_) || !SDL_SyncWindow(window_)) {
        return std::unexpected{std::string{SDL_GetError()}};
    }

    auto actual{display()};
    if (!actual) {
        return std::unexpected{std::move(actual.error())};
    }
    settings.set_running_display(*actual);
    return synchronize(true);
}

auto Window::synchronize_resize(PlayerSettings& settings) -> Result {
    if (SDL_GetWindowFlags(window_) & (SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED)) {
        return {};
    }

    auto actual{display()};
    if (!actual) {
        return std::unexpected{std::move(actual.error())};
    }
    settings.set_running_display(*actual);
    return synchronize(false);
}
}
