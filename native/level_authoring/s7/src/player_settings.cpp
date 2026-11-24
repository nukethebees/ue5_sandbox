#include <ioj/settings/player_settings.h>

#include <ioj/levels/authoring/game_configuration.h>
#include <ioj/settings/game_setting.h>
#include <ioj/settings/game_setting_category.h>
#include <ioj/settings/game_setting_choice.h>
#include <ioj/settings/game_setting_option_provider.h>
#include <ioj/settings/game_settings.h>
#include <ioj/settings/game_settings_apply_result.h>
#include <ioj/settings/game_settings_edit_state.h>
#include <ioj/settings/game_window_mode.h>
#include <ioj/settings/launch_display.h>
#include <ioj/settings/screen_resolution.h>
#include <ioj/settings/setting_control_kind.h>

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
                          std::optional<LaunchDisplay> launch) -> SettingsResult {
    scripts_ = std::move(scripts);
    personal_ = std::move(personal);
    desktop_ = desktop;
    launch_ = launch;

    auto app{levels::authoring::read_application_settings(scripts_)};
    if (!app) {
        return SettingsResult{std::unexpect, std::move(app.error())};
    }
    application_ = *app;
    return read_preferences();
}

auto PlayerSettings::read_preferences() -> SettingsResult {
    using levels::authoring::MissingPreferences;
    auto preferences{levels::authoring::read_user_settings(
        scripts_,
        personal_,
        desktop_,
        launch_ ? MissingPreferences::UseDefaults : MissingPreferences::CreateFile)};
    if (!preferences) {
        return SettingsResult{std::unexpect, std::move(preferences.error())};
    }

    saved_ = preferences->applied;
    defaults_ = preferences->defaults;
    return {};
}

auto PlayerSettings::saved_display() const -> HostDisplay {
    return {saved_.window_mode, saved_.resolution};
}

auto PlayerSettings::refresh(SettingsBackend const& backend, bool editing) -> SettingsResult {
    auto applied{saved_};
    auto defaults{defaults_};
    if (launch_ && *launch_ != LaunchDisplay::Headless) {
        auto actual{backend.inspect_display()};
        if (!actual) {
            return SettingsResult{std::unexpect, std::move(actual.error())};
        }
        applied.window_mode = actual->mode;
        applied.resolution = actual->resolution;
    }

    if (launch_) {
        if (*launch_ != LaunchDisplay::Preferences) {
            defaults.window_mode = applied.window_mode;
        } else if (defaults.window_mode == GameWindowMode::Fullscreen) {
            defaults.window_mode = GameWindowMode::Borderless;
        }
        if (*launch_ == LaunchDisplay::Headless ||
            defaults.window_mode == GameWindowMode::Borderless) {
            defaults.resolution = applied.resolution;
        }
    }

    if (editing) {
        session_.begin_edit(applied, defaults);
    } else {
        session_.initialise(applied, defaults);
    }
    return {};
}

auto PlayerSettings::activate(SettingsBackend const& backend) -> SettingsResult {
    if (auto result{refresh(backend, false)}; !result) {
        return result;
    }

    backend.apply_non_display(session_.applied_state());
    if (!launch_) {
        if (auto result{backend.apply_display(session_.applied_state())}; !result) {
            return result;
        }
        backend.confirm_display();
    }
    return {};
}

auto PlayerSettings::begin_edit(SettingsBackend const& backend) -> SettingsResult {
    if (session_.is_awaiting_display_confirmation()) {
        return {};
    }

    // PIE supports editing authored preferences on disk between visits to Options.
    if (!launch_) {
        if (auto result{read_preferences()}; !result) {
            return result;
        }

        if (auto result{backend.apply_display(saved_)}; !result) {
            return result;
        }
        backend.confirm_display();
    }

    if (auto result{refresh(backend, true)}; !result) {
        return result;
    }
    backend.apply_non_display(session_.applied_state());
    return {};
}

void PlayerSettings::cancel() {
    session_.cancel();
}

auto PlayerSettings::persist(GameSettingsState state) -> SettingsResult {
    if (launch_) {
        auto const& applied{session_.applied_state()};
        if (*launch_ != LaunchDisplay::Preferences || state.window_mode == applied.window_mode) {
            state.window_mode = saved_.window_mode;
        }
        if (session_.settings_state().window_mode != GameWindowMode::Windowed ||
            state.resolution == applied.resolution) {
            state.resolution = saved_.resolution;
        }
    }

    auto result{levels::authoring::save_user_settings(personal_, state, defaults_)};
    if (result) {
        saved_ = state;
    }
    return result;
}

auto PlayerSettings::apply(SettingsBackend const& backend, double now, bool bindings_changed)
    -> std::expected<GameSettingsApplyResult, std::string> {
    using ApplyResult = std::expected<GameSettingsApplyResult, std::string>;
    auto next{session_};
    auto const outcome{next.apply(now, bindings_changed)};
    if (outcome == GameSettingsApplyResult::Ignored) {
        return outcome;
    }

    bool const preview{outcome == GameSettingsApplyResult::ConfirmDisplay};
    if (preview) {
        if (auto result{backend.apply_display(next.settings_state())}; !result) {
            return ApplyResult{std::unexpect, std::move(result.error())};
        }
    }

    if (next.applied_state() != session_.applied_state()) {
        if (auto result{persist(next.applied_state())}; !result) {
            if (preview) {
                if (auto rollback{backend.revert_display()}; !rollback) {
                    result.error() += "\n" + rollback.error();
                }
            }
            return ApplyResult{std::unexpect, std::move(result.error())};
        }
    }

    backend.apply_non_display(next.settings_state());
    session_ = next;
    return outcome;
}

auto PlayerSettings::confirm_display(SettingsBackend const& backend) -> SettingsResult {
    if (!session_.is_awaiting_display_confirmation()) {
        return {};
    }

    if (auto result{backend.verify_display()}; !result) {
        if (auto rollback{revert_display(backend)}; !rollback) {
            result.error() += "\n" + rollback.error();
        }
        return result;
    }

    if (auto result{persist(session_.settings_state())}; !result) {
        return result;
    }
    backend.confirm_display();
    session_.confirm_display();
    return refresh(backend, true);
}

auto PlayerSettings::revert_display(SettingsBackend const& backend) -> SettingsResult {
    if (!session_.is_awaiting_display_confirmation()) {
        return {};
    }

    if (auto result{backend.revert_display()}; !result) {
        return result;
    }
    session_.revert_display();
    return refresh(backend, true);
}

auto PlayerSettings::is_available(GameSetting setting) const -> bool {
    if (launch_) {
        switch (setting) {
            case GameSetting::WindowMode:
                return *launch_ == LaunchDisplay::Preferences;
            case GameSetting::Resolution:
                return *launch_ != LaunchDisplay::Headless &&
                       session_.settings_state().window_mode == GameWindowMode::Windowed;
            default:
                break;
        }
    }

    return game_setting_is_available(game_setting_descriptor(setting).availability_provider,
                                     session_.settings_state());
}

auto PlayerSettings::choices(GameSettingOptionProvider provider,
                             std::vector<ScreenResolution> resolutions) const
    -> std::vector<GameSettingChoice> {
    if (launch_ && provider == GameSettingOptionProvider::SupportedResolutions) {
        resolutions.push_back(session_.settings_state().resolution);
    }

    auto choices{game_setting_choices(provider, resolutions)};
    if (launch_ && provider == GameSettingOptionProvider::WindowModes) {
        std::erase_if(choices, [](auto const& choice) {
            return choice.value == GameSettingValue{GameWindowMode::Fullscreen};
        });
    }
    return choices;
}

auto PlayerSettings::set_setting(GameSetting setting,
                                 GameSettingValue const& value,
                                 std::vector<ScreenResolution> resolutions) -> bool {
    if (!is_available(setting)) {
        return false;
    }
    auto const& descriptor{game_setting_descriptor(setting)};
    std::vector<GameSettingValue> allowed;
    if (descriptor.control_kind == SettingControlKind::Choice) {
        for (auto const& choice : choices(descriptor.options_provider, std::move(resolutions))) {
            allowed.push_back(choice.value);
        }
    }

    auto const previous{session_.settings_state().window_mode};
    auto normalized{normalize_game_setting_value(descriptor, value, allowed, true)};
    if (!normalized || !session_.set_setting(setting, *normalized)) {
        return false;
    }

    auto const mode{session_.settings_state().window_mode};
    if (launch_ && setting == GameSetting::WindowMode && mode != previous) {
        auto const& applied{session_.applied_state()};
        auto const restore_size{mode == GameWindowMode::Windowed && applied.window_mode != mode};
        session_.set_setting(GameSetting::Resolution,
                             restore_size ? saved_.resolution : applied.resolution);
    }
    return true;
}

auto PlayerSettings::reset_category(GameSettingCategory category) -> bool {
    return session_.reset_category(category);
}
}
