#include <ioj/settings/player_settings_view.h>

#include <ioj/settings/game_settings.h>

#include <algorithm>
#include <format>

namespace ioj {
auto PlayerSettingsView::is_available(GameSettingsState const& draft, GameSetting setting) const
    -> bool {
    switch (setting) {
        case GameSetting::WindowMode:
            return launch_mode == LaunchDisplayMode::Preferences;
        case GameSetting::Resolution:
            return launch_mode != LaunchDisplayMode::Headless &&
                   draft.video.window_mode == GameWindowMode::Windowed;
        default:
            break;
    }

    return game_setting_is_available(game_setting_descriptor(setting).availability_provider, draft);
}

auto PlayerSettingsView::choices(GameSettingsState const& draft,
                                 GameSettingOptionProvider provider,
                                 std::span<ScreenResolution const> resolutions) const
    -> std::vector<GameSettingChoice> {
    auto choices{game_setting_choices(provider, resolutions)};
    if (provider == GameSettingOptionProvider::SupportedResolutions &&
        std::ranges::find(resolutions, draft.video.resolution) == resolutions.end()) {
        choices.push_back(
            {draft.video.resolution,
             std::format("{} x {}", draft.video.resolution.width, draft.video.resolution.height)});
    }

    if (provider == GameSettingOptionProvider::WindowModes) {
        std::erase_if(choices, [](auto const& choice) {
            return choice.value == GameSettingValue{GameWindowMode::Fullscreen};
        });
    }

    return choices;
}

auto PlayerSettingsView::set_setting(GameSettingsEditState& edit,
                                     GameSetting setting,
                                     GameSettingValue const& value,
                                     std::span<ScreenResolution const> resolutions) const -> bool {
    auto const& draft{edit.pending()};
    if (!is_available(draft, setting)) {
        return false;
    }

    auto const& descriptor{game_setting_descriptor(setting)};
    std::vector<GameSettingValue> allowed;
    if (descriptor.control_kind == SettingControlKind::Choice) {
        for (auto const& choice : choices(draft, descriptor.options_provider, resolutions)) {
            allowed.push_back(choice.value);
        }
    }

    auto const previous{draft.video.window_mode};
    auto normalized{normalize_game_setting_value(descriptor, value, allowed, true)};
    if (!normalized || !edit.set_setting(setting, *normalized)) {
        return false;
    }

    auto const mode{draft.video.window_mode};
    if (setting == GameSetting::WindowMode && mode != previous) {
        auto const& applied{applied_values};
        auto const restore_size{mode == GameWindowMode::Windowed &&
                                applied.video.window_mode != mode};
        edit.set_setting(GameSetting::Resolution,
                         restore_size ? saved_resolution : applied.video.resolution);
    }

    return true;
}

}
