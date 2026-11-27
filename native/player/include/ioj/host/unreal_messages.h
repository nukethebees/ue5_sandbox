#pragma once

#include <ioj/host/input.h>
#include <ioj/settings/game_setting_category.h>
#include <ioj/settings/game_settings_state.h>

namespace ioj::host {
enum class PreferenceAction { Save, PreviewDisplay, RevertDisplay };

struct PreferenceEdit {
    PreferenceAction action;
    GameSettingCategory category;
    GameSettingsState values;
    GameSettingsState original_settings;
};

}
