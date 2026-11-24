#pragma once

#include <ioj/settings/game_window_mode.h>
#include <ioj/settings/screen_resolution.h>

namespace ioj {
enum class LaunchDisplay { Preferences, Windowed, Borderless, Headless };

struct HostDisplay {
    GameWindowMode mode{};
    ScreenResolution resolution{};
};
}
