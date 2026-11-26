#pragma once

#include "player/engine.h"
#include <ioj/settings/screen_resolution.h>
#include <ioj/settings/window_display_settings.h>

#include <SDL3/SDL_rect.h>

#include <expected>
#include <optional>
#include <string>
#include <vector>

struct SDL_Window;

namespace ioj {
class PlayerSettings;
}

namespace ioj::player {
class Window {
  public:
    Window() = default;
    ~Window();
    Window(Window const&) = delete;
    auto operator=(Window const&) -> Window& = delete;

    auto create() -> Result;
    auto desktop_resolution() const -> std::expected<ScreenResolution, std::string>;
    auto sdl_window() const -> SDL_Window* { return window_; }
    auto supported_resolutions() const -> std::vector<ScreenResolution> const& {
        return supported_resolutions_;
    }
    auto refresh_resolutions() -> Result;
    auto show(PlayerSettings& settings) -> Result;
    auto synchronize_resize(PlayerSettings& settings) -> Result;
    auto preview(WindowDisplaySettings display) -> Result;
    auto revert_preview() -> Result;
    auto verify_preview() const -> Result;
    void keep_preview(PlayerSettings& settings);
    auto configure(WindowDisplaySettings display)
        -> std::expected<WindowDisplaySettings, std::string>;

  private:
    auto synchronize(bool activate) -> Result;

    auto display() const -> std::expected<WindowDisplaySettings, std::string>;
    struct Preview {
        SDL_Rect floating;
        bool maximized;
        bool fullscreen;
        WindowDisplaySettings requested;
    };
    std::optional<Preview> preview_;
    SDL_Window* window_{};
    std::vector<ScreenResolution> supported_resolutions_;
};
}
