#pragma once

#include "../engine.h"
#include "library.h"
#include "system.h"
#include <ioj/host/game_host.h>
#include <ioj/host/game_host_api.h>
#include <ioj/settings/launch_display.h>
#include <ioj/settings/screen_resolution.h>

#include <expected>
#include <string>

namespace ioj::sandbox_host {
class Window {
  public:
    Window() = default;
    ~Window();
    Window(Window const&) = delete;
    auto operator=(Window const&) -> Window& = delete;

    auto create(HINSTANCE instance) -> Result;
    auto handle() const -> HWND;
    auto closed() const -> bool;
    auto show(Library const& library) -> Result;
    void disconnect();
    auto synchronize_resize() -> Result;
    auto resize(HostDisplay display) -> Result;
    static auto apply(void* context, DisplayOperation operation, HostDisplay& display)
        -> char const*;

  private:
    using DisplayResult = std::expected<HostDisplay, std::string>;

    struct Snapshot {
        LONG_PTR style;
        RECT bounds;
        WINDOWPLACEMENT placement;
        HostDisplay display;
    };

    static auto CALLBACK procedure(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) -> LRESULT;
    auto inspect() const -> DisplayResult;
    auto synchronize(bool activate) -> Result;
    auto set_bounds(LONG_PTR style, RECT bounds, UINT flags) -> Result;
    auto verify(HostDisplay display) -> Result;
    auto preview_display(HostDisplay display) -> Result;
    auto restore() -> Result;
    auto change(DisplayOperation operation, HostDisplay& display) -> Result;

    HWND handle_{};
    MessageHandler forward_{};
    decltype(&SandboxHost_InitializeWindow) sync_{};
    bool closed_{};
    bool size_changed_{};

    ScreenResolution synchronized_size_{};
    Snapshot previous_{};
    HostDisplay preview_{};
    std::string error_;
};
}
