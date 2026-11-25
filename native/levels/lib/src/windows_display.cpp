#include <ioj/settings/windows_display.h>

#include <ioj/system_error_message.h>

#include <climits>
#include <format>
#include <utility>

namespace ioj {
auto WindowsDisplay::inspect() const -> DisplayResult {
    RECT client{};
    if (!GetClientRect(handle_, &client)) {
        return DisplayResult{std::unexpect, system_error_message("GetClientRect", GetLastError())};
    }

    auto const style{GetWindowLongPtrW(handle_, GWL_STYLE)};
    if (!style) {
        return DisplayResult{std::unexpect,
                             system_error_message("GetWindowLongPtrW(GWL_STYLE)", GetLastError())};
    }

    auto const mode{(style & WS_POPUP) ? GameWindowMode::Borderless : GameWindowMode::Windowed};
    return DisplayResult{std::in_place, mode, ScreenResolution{client.right, client.bottom}};
}

auto WindowsDisplay::set_bounds(LONG_PTR style, RECT bounds, UINT flags) -> Result {
    SetLastError(0);
    if (!SetWindowLongPtrW(handle_, GWL_STYLE, style) && GetLastError()) {
        return Result{std::unexpect,
                      system_error_message("SetWindowLongPtrW(GWL_STYLE)", GetLastError())};
    }

    if (!SetWindowPos(handle_,
                      nullptr,
                      bounds.left,
                      bounds.top,
                      bounds.right - bounds.left,
                      bounds.bottom - bounds.top,
                      flags | SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOACTIVATE)) {
        return Result{std::unexpect, system_error_message("SetWindowPos", GetLastError())};
    }

    return {};
}

auto WindowsDisplay::verify_geometry(WindowDisplaySettings display) -> Result {
    auto actual{inspect()};
    if (!actual) {
        return Result{std::unexpect, std::move(actual.error())};
    }

    if (actual->mode != display.mode || actual->resolution != display.resolution) {
        return Result{std::unexpect,
                      std::format("Requested {}x{}, measured {}x{} client pixels",
                                  display.resolution.width,
                                  display.resolution.height,
                                  actual->resolution.width,
                                  actual->resolution.height)};
    }

    return {};
}

auto WindowsDisplay::resize(WindowDisplaySettings display) -> Result {
    bool const windowed{display.mode == GameWindowMode::Windowed};
    DWORD const style{windowed ? WS_OVERLAPPEDWINDOW : WS_POPUP};
    RECT bounds{};

    if (IsZoomed(handle_)) {
        ShowWindow(handle_, SW_RESTORE);
    }

    if (windowed) {
        if (!AdjustWindowRectExForDpi(&bounds, style, FALSE, 0, GetDpiForWindow(handle_))) {
            return Result{std::unexpect,
                          system_error_message("AdjustWindowRectExForDpi", GetLastError())};
        }

        if (display.resolution.width > LONG_MAX - (bounds.right - bounds.left) ||
            display.resolution.height > LONG_MAX - (bounds.bottom - bounds.top)) {
            return Result{std::unexpect, "Client size overflows the window bounds"};
        }

        bounds.right += display.resolution.width;
        bounds.bottom += display.resolution.height;
    } else {
        MONITORINFO monitor{};
        monitor.cbSize = sizeof(monitor);
        if (!GetMonitorInfoW(MonitorFromWindow(handle_, MONITOR_DEFAULTTONEAREST), &monitor)) {
            return Result{std::unexpect, system_error_message("GetMonitorInfoW", GetLastError())};
        }

        bounds = monitor.rcMonitor;
        display.mode = GameWindowMode::Borderless;
        display.resolution = {bounds.right - bounds.left, bounds.bottom - bounds.top};
    }

    auto result{set_bounds(
        style | (IsWindowVisible(handle_) ? WS_VISIBLE : 0), bounds, windowed ? SWP_NOMOVE : 0)};
    if (!result) {
        return result;
    }

    return verify_geometry(display);
}

auto WindowsDisplay::restore() -> Result {
    auto result{set_bounds(style_, bounds_, 0)};
    if (!result) {
        return result;
    }

    if (!SetWindowPlacement(handle_, &placement_)) {
        return Result{std::unexpect, system_error_message("SetWindowPlacement", GetLastError())};
    }

    return verify_geometry(previous_);
}

auto WindowsDisplay::preview_display(WindowDisplaySettings display) -> Result {
    auto actual{inspect()};
    if (!actual) {
        return Result{std::unexpect, std::move(actual.error())};
    }

    previous_ = *actual;
    style_ = GetWindowLongPtrW(handle_, GWL_STYLE);
    placement_.length = sizeof(WINDOWPLACEMENT);
    if (!style_ || !GetWindowRect(handle_, &bounds_) || !GetWindowPlacement(handle_, &placement_)) {
        return Result{std::unexpect,
                      system_error_message("Capture previous window geometry", GetLastError())};
    }

    auto result{resize(display)};
    if (result) {
        auto measured{inspect()};
        if (measured) {
            preview_ = *measured;
            return {};
        }
        result = Result{std::unexpect, std::move(measured.error())};
    }
    if (auto rollback{restore()}; !rollback) {
        result.error() += "; restoring previous geometry failed: " + rollback.error();
    }

    return result;
}

}
