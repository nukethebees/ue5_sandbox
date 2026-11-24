#pragma once

#include <ioj/host/game_host.h>
#include <ioj/settings/player_settings.h>

namespace ioj {
auto take_prepared_settings() -> PlayerSettings;
auto game_host() -> GameHost&;
}
