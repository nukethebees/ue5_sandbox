#pragma once

#include <ioj/settings/game_setting_value.h>

#include <string>

namespace ioj {
struct GameSettingChoice {
    GameSettingValue value;
    std::string label;
};
}
