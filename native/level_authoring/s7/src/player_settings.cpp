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

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ioj {
auto PlayerSettings::load(std::filesystem::path scripts,
                          std::filesystem::path personal,
                          ScreenResolution desktop,
                          std::optional<LaunchDisplayMode> launch) -> SettingsResult {
    auto app{levels::authoring::read_application_settings(scripts)};
    if (!app) {
        return SettingsResult{std::unexpect, std::move(app.error())};
    }
    auto preferences{levels::authoring::read_user_settings(scripts, personal, desktop)};
    if (!preferences) {
        return SettingsResult{std::unexpect, std::move(preferences.error())};
    }

    personal_ = std::move(personal);
    application_ = *app;
    launch_ = launch;
    applied_ = preferences->applied;
    saved_ = preferences->applied;
    defaults_ = preferences->defaults;
    return {};
}

auto PlayerSettings::saved_display() const -> WindowDisplaySettings {
    return {saved_.window_mode, saved_.resolution};
}

void PlayerSettings::set_running_display(WindowDisplaySettings display) {
    applied_.window_mode = display.mode;
    applied_.resolution = display.resolution;
}

auto video_settings(GameSettingsState const& state) -> VideoSettings {
    return {
        .resolution = state.resolution,
        .window_mode = state.window_mode,
        .vsync = state.vsync,
        .frame_rate_limit = state.frame_rate_limit,
        .resolution_scale = state.resolution_scale,
        .overall_quality = state.overall_quality,
        .view_distance_quality = state.view_distance_quality,
        .aa_method = state.aa_method,
        .aa_quality = state.aa_quality,
        .shadow_quality = state.shadow_quality,
        .global_illumination_quality = state.global_illumination_quality,
        .reflections_quality = state.reflections_quality,
        .post_processing_quality = state.post_processing_quality,
        .texture_quality = state.texture_quality,
        .effects_quality = state.effects_quality,
        .shading_quality = state.shading_quality,
        .bloom = state.bloom,
        .motion_blur = state.motion_blur,
    };
}

void PlayerSettings::apply(VideoSettings const& values) {
    applied_.resolution = values.resolution;
    applied_.window_mode = values.window_mode;
    applied_.vsync = values.vsync;
    applied_.frame_rate_limit = values.frame_rate_limit;
    applied_.resolution_scale = values.resolution_scale;
    applied_.overall_quality = values.overall_quality;
    applied_.view_distance_quality = values.view_distance_quality;
    applied_.aa_method = values.aa_method;
    applied_.aa_quality = values.aa_quality;
    applied_.shadow_quality = values.shadow_quality;
    applied_.global_illumination_quality = values.global_illumination_quality;
    applied_.reflections_quality = values.reflections_quality;
    applied_.post_processing_quality = values.post_processing_quality;
    applied_.texture_quality = values.texture_quality;
    applied_.effects_quality = values.effects_quality;
    applied_.shading_quality = values.shading_quality;
    applied_.bloom = values.bloom;
    applied_.motion_blur = values.motion_blur;
}

auto audio_settings(GameSettingsState const& state) -> AudioSettings {
    return {
        .master_volume = state.master_volume,
        .music_volume = state.music_volume,
        .sfx_volume = state.sfx_volume,
        .ui_volume = state.ui_volume,
    };
}

void PlayerSettings::apply(AudioSettings const& values) {
    applied_.master_volume = values.master_volume;
    applied_.music_volume = values.music_volume;
    applied_.sfx_volume = values.sfx_volume;
    applied_.ui_volume = values.ui_volume;
}

auto controls_settings(GameSettingsState const& state) -> ControlsSettings {
    return {
        .mouse_turn_sensitivity = state.mouse_turn_sensitivity,
        .gamepad_turn_sensitivity = state.gamepad_turn_sensitivity,
        .gamepad_turn_dead_zone = state.gamepad_turn_dead_zone,
        .invert_mouse_pitch = state.invert_mouse_pitch,
        .invert_mouse_yaw = state.invert_mouse_yaw,
        .invert_gamepad_pitch = state.invert_gamepad_pitch,
        .invert_gamepad_yaw = state.invert_gamepad_yaw,
    };
}

void PlayerSettings::apply(ControlsSettings const& values) {
    applied_.mouse_turn_sensitivity = values.mouse_turn_sensitivity;
    applied_.gamepad_turn_sensitivity = values.gamepad_turn_sensitivity;
    applied_.gamepad_turn_dead_zone = values.gamepad_turn_dead_zone;
    applied_.invert_mouse_pitch = values.invert_mouse_pitch;
    applied_.invert_mouse_yaw = values.invert_mouse_yaw;
    applied_.invert_gamepad_pitch = values.invert_gamepad_pitch;
    applied_.invert_gamepad_yaw = values.invert_gamepad_yaw;
}

auto PlayerSettings::save_category(GameSettingCategory category,
                                   GameSettingsState const& state,
                                   GameSettingsState const& before) -> SettingsResult {
    auto next{saved_};
    for (auto const& descriptor : game_setting_descriptors()) {
        if (descriptor.category != category) {
            continue;
        }
        auto const value{game_setting_value(state, descriptor.id)};
        if (value != game_setting_value(before, descriptor.id)) {
            set_game_setting_value(next, descriptor.id, value);
        }
    }

    if (next == saved_) {
        return {};
    }
    auto result{levels::authoring::save_user_settings(personal_, next, defaults_)};
    if (result) {
        saved_ = next;
    }
    return result;
}

auto PlayerSettings::is_available(GameSettingsState const& draft, GameSetting setting) const
    -> bool {
    if (launch_) {
        switch (setting) {
            case GameSetting::WindowMode:
                return *launch_ == LaunchDisplayMode::Preferences;
            case GameSetting::Resolution:
                return *launch_ != LaunchDisplayMode::Headless &&
                       draft.window_mode == GameWindowMode::Windowed;
            default:
                break;
        }
    }

    return game_setting_is_available(game_setting_descriptor(setting).availability_provider, draft);
}

auto PlayerSettings::choices(GameSettingsState const& draft,
                             GameSettingOptionProvider provider,
                             std::vector<ScreenResolution> resolutions) const
    -> std::vector<GameSettingChoice> {
    if (launch_ && provider == GameSettingOptionProvider::SupportedResolutions) {
        resolutions.push_back(draft.resolution);
    }

    auto choices{game_setting_choices(provider, resolutions)};
    if (launch_ && provider == GameSettingOptionProvider::WindowModes) {
        std::erase_if(choices, [](auto const& choice) {
            return choice.value == GameSettingValue{GameWindowMode::Fullscreen};
        });
    }
    return choices;
}

auto PlayerSettings::set_setting(GameSettingsEditState& edit,
                                 GameSetting setting,
                                 GameSettingValue const& value,
                                 std::vector<ScreenResolution> resolutions) const -> bool {
    auto const& draft{edit.pending()};
    if (!is_available(draft, setting)) {
        return false;
    }
    auto const& descriptor{game_setting_descriptor(setting)};
    std::vector<GameSettingValue> allowed;
    if (descriptor.control_kind == SettingControlKind::Choice) {
        for (auto const& choice :
             choices(draft, descriptor.options_provider, std::move(resolutions))) {
            allowed.push_back(choice.value);
        }
    }

    auto const previous{draft.window_mode};
    auto normalized{normalize_game_setting_value(descriptor, value, allowed, true)};
    if (!normalized || !edit.set_setting(setting, *normalized)) {
        return false;
    }

    auto const mode{draft.window_mode};
    if (launch_ && setting == GameSetting::WindowMode && mode != previous) {
        auto const& applied{applied_};
        auto const restore_size{mode == GameWindowMode::Windowed && applied.window_mode != mode};
        edit.set_setting(GameSetting::Resolution,
                         restore_size ? saved_.resolution : applied.resolution);
    }
    return true;
}

}
