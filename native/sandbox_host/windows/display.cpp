#include "../engine.h"
#include "system.h"
#include "window.h"
#include <ioj/host/game_host.h>
#include <ioj/settings/game_window_mode.h>
#include <ioj/settings/launch_display.h>
#include <ioj/settings/screen_resolution.h>

#include <climits>
#include <expected>
#include <format>
#include <utility>

namespace ioj::sandbox_host {
auto Window::inspect() const -> DisplayResult {
    RECT client{};
    if (!GetClientRect(handle_, &client)) {
        return DisplayResult{std::unexpect, system_error("GetClientRect", GetLastError())};
    }

    auto const style{GetWindowLongPtrW(handle_, GWL_STYLE)};
    if (!style) {
        return DisplayResult{std::unexpect,
                             system_error("GetWindowLongPtrW(GWL_STYLE)", GetLastError())};
    }

    auto const mode{(style & WS_POPUP) ? GameWindowMode::Borderless : GameWindowMode::Windowed};
    return DisplayResult{std::in_place, mode, ScreenResolution{client.right, client.bottom}};
}

// Match the Slate and game viewport to the measured client area.
auto Window::synchronize(bool activate) -> Result {
    auto actual{inspect()};
    if (!actual) {
        return Result{std::unexpect, std::move(actual.error())};
    }

    POINT origin{};
    if (!ClientToScreen(handle_, &origin)) {
        return Result{std::unexpect, system_error("ClientToScreen", GetLastError())};
    }

    if (sync_ &&
        !sync_(origin.x, origin.y, actual->resolution.width, actual->resolution.height, activate)) {
        return Result{std::unexpect, "Slate or Unreal viewport differs from the host client area"};
    }

    synchronized_size_ = actual->resolution;
    size_changed_ = false;

    return {};
}

auto Window::set_bounds(LONG_PTR style, RECT bounds, UINT flags) -> Result {
    SetLastError(0);
    if (!SetWindowLongPtrW(handle_, GWL_STYLE, style) && GetLastError()) {
        return Result{std::unexpect, system_error("SetWindowLongPtrW(GWL_STYLE)", GetLastError())};
    }

    if (!SetWindowPos(handle_,
                      nullptr,
                      bounds.left,
                      bounds.top,
                      bounds.right - bounds.left,
                      bounds.bottom - bounds.top,
                      flags | SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOACTIVATE)) {
        return Result{std::unexpect, system_error("SetWindowPos", GetLastError())};
    }

    return {};
}

auto Window::verify(HostDisplay display) -> Result {
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

    return synchronize(false);
}

auto Window::resize(HostDisplay display) -> Result {
    bool const windowed{display.mode == GameWindowMode::Windowed};
    DWORD const style{windowed ? WS_OVERLAPPEDWINDOW : WS_POPUP};
    RECT bounds{};

    if (IsZoomed(handle_)) {
        ShowWindow(handle_, SW_RESTORE);
    }

    if (windowed) {
        if (!AdjustWindowRectExForDpi(&bounds, style, FALSE, 0, GetDpiForWindow(handle_))) {
            return Result{std::unexpect, system_error("AdjustWindowRectExForDpi", GetLastError())};
        }

        if (display.resolution.width > LONG_MAX - (bounds.right - bounds.left) ||
            display.resolution.height > LONG_MAX - (bounds.bottom - bounds.top)) {
            return Result{std::unexpect, "Client size overflows the window bounds"};
        }

        bounds.right += display.resolution.width;
        bounds.bottom += display.resolution.height;
    } else {
        MONITORINFO monitor{sizeof(MONITORINFO)};
        if (!GetMonitorInfoW(MonitorFromWindow(handle_, MONITOR_DEFAULTTONEAREST), &monitor)) {
            return Result{std::unexpect, system_error("GetMonitorInfoW", GetLastError())};
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

    return verify(display);
}

auto Window::restore() -> Result {
    auto result{set_bounds(previous_.style, previous_.bounds, 0)};
    if (!result) {
        return result;
    }

    if (!SetWindowPlacement(handle_, &previous_.placement)) {
        return Result{std::unexpect, system_error("SetWindowPlacement", GetLastError())};
    }

    return verify(previous_.display);
}

auto Window::change(DisplayOperation operation, HostDisplay& display) -> Result {
    switch (operation) {
        case DisplayOperation::Revert: {
            display = previous_.display;
            return restore();
        }
        case DisplayOperation::Verify: {
            display = preview_;
            return verify(preview_);
        }
        case DisplayOperation::Inspect: {
            auto actual{inspect()};
            if (!actual) {
                return Result{std::unexpect, std::move(actual.error())};
            }

            display = *actual;
            return {};
        }
        case DisplayOperation::Preview: {
            return preview_display(display);
        }
    }
    std::unreachable();
}

auto Window::preview_display(HostDisplay display) -> Result {
    auto actual{inspect()};
    if (!actual) {
        return Result{std::unexpect, std::move(actual.error())};
    }

    previous_.display = *actual;
    previous_.style = GetWindowLongPtrW(handle_, GWL_STYLE);
    previous_.placement.length = sizeof(WINDOWPLACEMENT);
    if (!previous_.style || !GetWindowRect(handle_, &previous_.bounds) ||
        !GetWindowPlacement(handle_, &previous_.placement)) {
        return Result{std::unexpect,
                      system_error("Capture previous window geometry", GetLastError())};
    }

    auto result{resize(display)};
    if (result) {
        preview_ = {display.mode, synchronized_size_};
    } else if (auto rollback{restore()}; !rollback) {
        result.error() += "; restoring previous geometry failed: " + rollback.error();
    }

    return result;
}

auto Window::apply(void* context, DisplayOperation operation, HostDisplay& display) -> char const* {
    auto& window{*static_cast<Window*>(context)};
    if (auto result{window.change(operation, display)}; !result) {
        window.error_ = std::move(result.error());
        return window.error_.c_str();
    }

    return nullptr;
}
}
