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
#include <optional>
#include <string>
#include <vector>

namespace ioj {
using SettingsResult = std::expected<void, std::string>;

auto video_settings(GameSettingsState const& state) -> VideoSettings;
auto audio_settings(GameSettingsState const& state) -> AudioSettings;
auto controls_settings(GameSettingsState const& state) -> ControlsSettings;

class PlayerSettings {
  public:
    auto load(std::filesystem::path scripts,
              std::filesystem::path personal,
              ScreenResolution desktop,
              std::optional<LaunchDisplayMode> launch) -> SettingsResult;

    auto application() const -> ApplicationSettings const& { return application_; }
    auto applied() const -> GameSettingsState const& { return applied_; }
    auto defaults() const -> GameSettingsState const& { return defaults_; }
    auto launch_display() const -> std::optional<LaunchDisplayMode> { return launch_; }
    auto loaded() const -> bool { return !personal_.empty(); }
    void set_running_display(WindowDisplaySettings display);
    void apply(VideoSettings const& values);
    void apply(AudioSettings const& values);
    void apply(ControlsSettings const& values);
    auto save_category(GameSettingCategory category,
                       GameSettingsState const& state,
                       GameSettingsState const& before) -> SettingsResult;

    auto saved_display() const -> WindowDisplaySettings;
    auto is_available(GameSettingsState const& draft, GameSetting setting) const -> bool;
    auto choices(GameSettingsState const& draft,
                 GameSettingOptionProvider provider,
                 std::vector<ScreenResolution> resolutions) const -> std::vector<GameSettingChoice>;
    auto set_setting(GameSettingsEditState& draft,
                     GameSetting setting,
                     GameSettingValue const& value,
                     std::vector<ScreenResolution> resolutions) const -> bool;

  private:
    std::filesystem::path personal_;
    std::optional<LaunchDisplayMode> launch_;

    ApplicationSettings application_{};
    GameSettingsState saved_{};
    GameSettingsState defaults_{};
    GameSettingsState applied_{};
};
}
