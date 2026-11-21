#pragma once

#include <ioj/settings/application_settings.h>
#include <ioj/settings/game_settings_state.h>

#include <expected>
#include <filesystem>
#include <string>

namespace ioj::levels::authoring {
struct UserSettings {
    GameSettingsState defaults;
    GameSettingsState applied;
};
using UserSettingsResult = std::expected<UserSettings, std::string>;
using ApplicationSettingsResult = std::expected<ApplicationSettings, std::string>;
using SavePreferencesResult = std::expected<void, std::string>;

auto read_application_settings(std::filesystem::path const& script_root)
    -> ApplicationSettingsResult;
auto read_user_settings(std::filesystem::path const& script_root,
                        std::filesystem::path const& personal_file,
                        ScreenResolution desktop) -> UserSettingsResult;
auto save_user_settings(std::filesystem::path const& personal_file,
                        GameSettingsState const& state,
                        GameSettingsState const& defaults) -> SavePreferencesResult;
}
