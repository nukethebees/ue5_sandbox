#include <ioj/settings/player_settings.h>

#include <ioj/levels/authoring/game_configuration.h>
#include <ioj/settings/game_setting.h>
#include <ioj/settings/game_setting_category.h>
#include <ioj/settings/game_setting_choice.h>
#include <ioj/settings/game_setting_option_provider.h>
#include <ioj/settings/game_settings.h>
#include <ioj/settings/game_settings_edit_state.h>
#include <ioj/settings/game_window_mode.h>
#include <ioj/settings/launch_display_mode.h>
#include <ioj/settings/screen_resolution.h>
#include <ioj/settings/setting_control_kind.h>
#include <ioj/settings/window_display_settings.h>

#include <algorithm>
#include <expected>
#include <filesystem>
#include <format>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace ioj {
auto PlayerSettings::load(std::filesystem::path const& scripts,
                          std::filesystem::path user_settings_path,
                          ScreenResolution desktop,
                          LaunchDisplayMode launch_display_mode) -> SettingsResult {
    auto app{levels::authoring::read_application_settings(scripts)};
    if (!app) {
        return SettingsResult{std::unexpect, std::move(app.error())};
    }

    auto preferences{levels::authoring::read_user_settings(scripts, user_settings_path, desktop)};
    if (!preferences) {
        return SettingsResult{std::unexpect, std::move(preferences.error())};
    }

    user_settings_path_ = std::move(user_settings_path);
    application_settings_ = *app;
    launch_display_mode_ = launch_display_mode;
    applied_settings_ = preferences->applied;
    saved_settings_ = preferences->applied;
    default_settings_ = preferences->defaults;
    return {};
}

auto PlayerSettings::saved_display() const -> WindowDisplaySettings {
    return {saved_settings_.video.window_mode, saved_settings_.video.resolution};
}

void PlayerSettings::set_running_display(WindowDisplaySettings display) {
    applied_settings_.video.window_mode = display.mode;
    applied_settings_.video.resolution = display.resolution;
}

void PlayerSettings::apply(VideoSettings const& values) {
    applied_settings_.video = values;
}

void PlayerSettings::apply(AudioSettings const& values) {
    applied_settings_.audio = values;
}

void PlayerSettings::apply(ControlsSettings const& values) {
    applied_settings_.controls = values;
}

auto PlayerSettings::save_category(GameSettingCategory category,
                                   GameSettingsState const& state,
                                   GameSettingsState const& original_settings) -> SettingsResult {
    auto next{saved_settings_};
    for (auto const& descriptor : game_setting_descriptors()) {
        if (descriptor.category != category) {
            continue;
        }

        auto const value{game_setting_value(state, descriptor.id)};
        if (value != game_setting_value(original_settings, descriptor.id)) {
            set_game_setting_value(next, descriptor.id, value);
        }
    }

    if (next == saved_settings_) {
        return {};
    }

    auto result{
        levels::authoring::save_user_settings(user_settings_path_, next, default_settings_)};
    if (result) {
        saved_settings_ = next;
    }

    return result;
}

}
