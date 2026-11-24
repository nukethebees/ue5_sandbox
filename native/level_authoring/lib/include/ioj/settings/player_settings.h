#pragma once

#include <ioj/settings/application_settings.h>
#include <ioj/settings/game_setting.h>
#include <ioj/settings/game_setting_category.h>
#include <ioj/settings/game_setting_choice.h>
#include <ioj/settings/game_setting_option_provider.h>
#include <ioj/settings/game_setting_value.h>
#include <ioj/settings/game_settings_apply_result.h>
#include <ioj/settings/game_settings_session.h>
#include <ioj/settings/game_settings_state.h>
#include <ioj/settings/launch_display.h>
#include <ioj/settings/screen_resolution.h>

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ioj {
using SettingsResult = std::expected<void, std::string>;

class SettingsBackend {
  public:
    virtual ~SettingsBackend() = default;
    virtual auto inspect_display() const -> std::expected<HostDisplay, std::string> = 0;
    virtual auto apply_display(GameSettingsState const& state) const -> SettingsResult = 0;
    virtual auto verify_display() const -> SettingsResult = 0;
    virtual void confirm_display() const = 0;
    virtual auto revert_display() const -> SettingsResult = 0;
    virtual void apply_non_display(GameSettingsState const& state) const = 0;
};

class PlayerSettings {
  public:
    auto load(std::filesystem::path scripts,
              std::filesystem::path personal,
              ScreenResolution desktop,
              std::optional<LaunchDisplay> launch) -> SettingsResult;

    auto activate(SettingsBackend const& backend) -> SettingsResult;
    auto begin_edit(SettingsBackend const& backend) -> SettingsResult;
    void cancel();
    auto apply(SettingsBackend const& backend, double now, bool bindings_changed)
        -> std::expected<GameSettingsApplyResult, std::string>;
    auto confirm_display(SettingsBackend const& backend) -> SettingsResult;
    auto revert_display(SettingsBackend const& backend) -> SettingsResult;

    auto application() const -> ApplicationSettings const& { return application_; }
    auto session() const -> GameSettingsSession const& { return session_; }
    auto saved_display() const -> HostDisplay;
    auto is_available(GameSetting setting) const -> bool;
    auto choices(GameSettingOptionProvider provider,
                 std::vector<ScreenResolution> resolutions) const -> std::vector<GameSettingChoice>;
    auto set_setting(GameSetting setting,
                     GameSettingValue const& value,
                     std::vector<ScreenResolution> resolutions) -> bool;
    auto reset_category(GameSettingCategory category) -> bool;

  private:
    auto refresh(SettingsBackend const& backend, bool editing) -> SettingsResult;
    auto read_preferences() -> SettingsResult;
    auto persist(GameSettingsState state) -> SettingsResult;

    std::filesystem::path scripts_;
    std::filesystem::path personal_;
    ScreenResolution desktop_{};
    std::optional<LaunchDisplay> launch_;

    ApplicationSettings application_{};
    GameSettingsState saved_{};
    GameSettingsState defaults_{};
    GameSettingsSession session_;
};
}
