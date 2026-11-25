#include "windows/window.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_video.h>

namespace ioj::sandbox_host {
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
        SDL_CreateWindow("Sandbox",
                         startup_size.width,
                         startup_size.height,
                         SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window_) {
        return Result{std::unexpect, SDL_GetError()};
    }
    return {};
}

auto Window::desktop_resolution() const -> std::expected<ScreenResolution, std::string> {
    auto const* mode{SDL_GetDesktopDisplayMode(SDL_GetDisplayForWindow(window_))};
    if (!mode) {
        return std::unexpected{std::string{SDL_GetError()}};
    }
    return ScreenResolution{mode->w, mode->h};
}
}
