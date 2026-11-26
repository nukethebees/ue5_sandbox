#pragma once

#include <ioj/settings/application_settings.h>
#include <ioj/settings/audio_settings.h>
#include <ioj/settings/controls_settings.h>
#include <ioj/settings/game_setting.h>
#include <ioj/settings/game_setting_category.h>
#include <ioj/settings/game_setting_choice.h>
#include <ioj/settings/game_setting_option_provider.h>
#include <ioj/settings/game_settings_edit_state.h>
#include <ioj/settings/game_settings_state.h>
#include <ioj/settings/launch_display_mode.h>
#include <ioj/settings/screen_resolution.h>
#include <ioj/settings/video_settings.h>
#include <ioj/settings/window_display_settings.h>

#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace ioj {
using SettingsResult = std::expected<void, std::string>;

class PlayerSettings {
  public:
    auto load(std::filesystem::path const& scripts,
              std::filesystem::path user_settings_path,
              ScreenResolution desktop,
              LaunchDisplayMode launch_display_mode) -> SettingsResult;

    auto application() const -> ApplicationSettings const& { return application_settings_; }
    auto applied() const -> GameSettingsState const& { return applied_settings_; }
    auto defaults() const -> GameSettingsState const& { return default_settings_; }
    auto launch_display_mode() const -> LaunchDisplayMode { return launch_display_mode_; }
    void set_running_display(WindowDisplaySettings display);
    void apply(VideoSettings const& values);
    void apply(AudioSettings const& values);
    void apply(ControlsSettings const& values);
    auto save_category(GameSettingCategory category,
                       GameSettingsState const& state,
                       GameSettingsState const& original_settings) -> SettingsResult;

    auto saved_display() const -> WindowDisplaySettings;
    auto is_available(GameSettingsState const& draft, GameSetting setting) const -> bool;
    auto choices(GameSettingsState const& draft,
                 GameSettingOptionProvider provider,
                 std::span<ScreenResolution const> resolutions) const
        -> std::vector<GameSettingChoice>;
    auto set_setting(GameSettingsEditState& edit,
                     GameSetting setting,
                     GameSettingValue const& value,
                     std::span<ScreenResolution const> resolutions) const -> bool;

  private:
    std::filesystem::path user_settings_path_;
    LaunchDisplayMode launch_display_mode_{LaunchDisplayMode::Preferences};

    ApplicationSettings application_settings_{};
    GameSettingsState saved_settings_{};
    GameSettingsState default_settings_{};
    GameSettingsState applied_settings_{};
};
}
