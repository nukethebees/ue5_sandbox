#pragma once

#include <ioj/settings/game_setting_value.h>

#include <Internationalization/Text.h>

namespace ioj {
struct SPACEGAME_API FGameSettingOption {
    GameSettingValue value;
    FText label;
};
}
