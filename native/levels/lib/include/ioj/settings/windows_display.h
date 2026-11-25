#pragma once
#include <ioj/settings/window_display_settings.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <expected>
#include <string>

namespace ioj {
class WindowsDisplay {
  public:
    using Result = std::expected<void, std::string>;
    using DisplayResult = std::expected<WindowDisplaySettings, std::string>;

    explicit WindowsDisplay(HWND handle)
        : handle_{handle} {}
    auto inspect() const -> DisplayResult;
    auto resize(WindowDisplaySettings display) -> Result;
    auto preview_display(WindowDisplaySettings display) -> Result;
    auto verify_preview() -> Result { return verify_geometry(preview_); }
    auto restore() -> Result;

  private:
    auto verify_geometry(WindowDisplaySettings display) -> Result;
    auto set_bounds(LONG_PTR style, RECT bounds, UINT flags) -> Result;
    HWND handle_;
    LONG_PTR style_{};
    RECT bounds_{};
    WINDOWPLACEMENT placement_{};
    WindowDisplaySettings previous_{};
    WindowDisplaySettings preview_{};
};
}
