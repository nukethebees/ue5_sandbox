#pragma once

#include "../engine.h"
#include "library.h"
#include "system.h"
#include <ioj/host/game_host_api.h>
#include <ioj/settings/screen_resolution.h>
#include <ioj/settings/window_display_settings.h>

#include <expected>
#include <string>

struct SDL_Window;

namespace ioj::sandbox_host {
class Window {
  public:
    Window() = default;
    ~Window();
    Window(Window const&) = delete;
    auto operator=(Window const&) -> Window& = delete;

    auto create() -> Result;
    auto desktop_resolution() const -> std::expected<ScreenResolution, std::string>;
    auto handle() const -> HWND;
    auto closed() const -> bool;
    auto show(Library const& library) -> Result;
    void disconnect();
    auto synchronize_resize() -> Result;
    auto resize(WindowDisplaySettings display) -> Result;

  private:
    static auto CALLBACK procedure(
        HWND hwnd, UINT message, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR context) -> LRESULT;
    auto synchronize(bool activate) -> Result;

    SDL_Window* window_{};
    MessageHandler forward_{};
    decltype(&SandboxHost_InitializeWindow) sync_{};
    bool closed_{};
    bool size_changed_{};
};
}
