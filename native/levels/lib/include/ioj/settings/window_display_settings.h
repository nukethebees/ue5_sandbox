#pragma once

#include <ioj/settings/game_window_mode.h>
#include <ioj/settings/screen_resolution.h>

namespace ioj {
struct WindowDisplaySettings {
    GameWindowMode mode{};
    ScreenResolution resolution{};
};
}
