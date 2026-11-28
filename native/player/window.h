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

class Window {
  public:
    ~Window();
    Window(Window const&) = delete;
    auto operator=(Window const&) -> Window& = delete;

    Window(Window&& other) noexcept;
    auto operator=(Window&& other) noexcept -> Window&;
    static auto create() -> std::expected<Window, std::string>;
    auto desktop_resolution() const -> std::expected<ScreenResolution, std::string>;
    auto sdl_window() const -> SDL_Window* { return window_; }
    auto supported_resolutions() const -> std::vector<ScreenResolution> const& {
        return supported_resolutions_;
    }
    auto refresh_resolutions() -> ApplicationResult;
    auto show(PlayerSettings& settings) -> ApplicationResult;
    auto synchronize_resize(PlayerSettings& settings) -> ApplicationResult;
    auto preview(WindowDisplaySettings display) -> ApplicationResult;
    auto revert_preview() -> ApplicationResult;
    auto verify_preview() const -> ApplicationResult;
    void keep_preview(PlayerSettings& settings);
    auto configure(WindowDisplaySettings display)
        -> std::expected<WindowDisplaySettings, std::string>;

  private:
    explicit Window(SDL_Window* window)
        : window_{window} {}
    auto synchronize(bool activate) -> ApplicationResult;

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
