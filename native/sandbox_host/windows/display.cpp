#include "../engine.h"
#include "system.h"
#include "window.h"
#include <ioj/settings/game_window_mode.h>
#include <ioj/settings/screen_resolution.h>
#include <ioj/settings/window_display_settings.h>
#include <ioj/settings/windows_display.h>
#include <ioj/system_error_message.h>

#include <expected>
#include <utility>

namespace ioj::sandbox_host {
// Match the Slate and game viewport to the measured client area.
auto Window::synchronize(bool activate) -> Result {
    auto const hwnd{handle()};
    auto actual{WindowsDisplay{hwnd}.inspect()};
    if (!actual) {
        return Result{std::unexpect, std::move(actual.error())};
    }

    POINT origin{};
    if (!ClientToScreen(hwnd, &origin)) {
        return Result{std::unexpect, system_error_message("ClientToScreen", GetLastError())};
    }

    if (sync_ &&
        !sync_(origin.x, origin.y, actual->resolution.width, actual->resolution.height, activate)) {
        return Result{std::unexpect, "Slate or Unreal viewport differs from the host client area"};
    }

    size_changed_ = false;

    return {};
}

auto Window::resize(WindowDisplaySettings display) -> Result {
    return WindowsDisplay{handle()}.resize(display);
}
}
