#include "SpaceGame/settings/OptionsDraft.h"

#include "SpaceGame/settings/GameSettingOption.h"
#include "SpaceGame/UnrealMessages.h"
#include <ioj/settings/game_setting.h>
#include <ioj/settings/game_setting_category.h>
#include <ioj/settings/game_setting_option_provider.h>
#include <ioj/settings/game_setting_value.h>
#include <ioj/settings/game_settings.h>
#include <ioj/settings/player_settings_view.h>
#include <SpaceGamePresentation/audio/GameAudio.h>

#include <SandboxCoreEngine/strings.h>

#include "Containers/Array.h"
#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "HAL/Platform.h"
#include "Logging/LogMacros.h"
#include "Logging/LogVerbosity.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Optional.h"
#include "Templates/Casts.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/Object.h"
#include <Engine/GameInstance.h>
#include <Misc/App.h>
#include <Misc/MessageDialog.h>

#include <string>
#include <variant>

namespace ioj {
namespace {
void report_settings_error(std::string const& error) {
    auto const message{FText::FromString(to_fstring(error))};
    UE_LOG(LogTemp, Error, TEXT("%s"), *message.ToString());
    if (!FApp::IsUnattended() && !GIsAutomationTesting) {
        FMessageDialog::Open(EAppMsgType::Ok, message);
    }
}
}

/* **************************************** */
// Lifecycle
/* **************************************** */
void UOptionsDraft::load(ULocalPlayer* player) {
    editing_local_player_ = player;
}

auto UOptionsDraft::apply(GameSettingCategory category) -> bool {
    if (player_menu().saving || !editing_local_player_.IsValid() ||
        player_menu().prompt == OptionsPrompt::Display || !player_menu().draft.is_dirty()) {
        return false;
    }

    auto const& pending{player_menu().draft.pending()};
    auto const& applied{player_menu().draft.applied()};
    if (category == GameSettingCategory::Video &&
        (pending.video.window_mode != applied.video.window_mode ||
         pending.video.resolution != applied.video.resolution)) {
        player_menu().saving = true;
        queue_preferences({host::PreferenceAction::PreviewDisplay, category, pending, applied},
                          *this);
        return true;
    }

    return save(category);
}

auto UOptionsDraft::save(GameSettingCategory category) -> bool {
    if (auto result{apply_settings(category, player_menu().draft.pending())}; !result) {
        report_settings_error("Applying settings failed: " + result.error());
        return false;
    }

    queue_preferences({host::PreferenceAction::Save,
                       category,
                       player_menu().draft.pending(),
                       player_menu().draft.applied()},
                      *this);
    player_menu().saving = true;
    return true;
}

void UOptionsDraft::preference_finished(host::PreferenceEdit const& edit, std::string_view error) {
    if (!error.empty()) {
        std::string message{error};
        if (edit.action == host::PreferenceAction::Save) {
            auto const restored{apply_settings(edit.category, edit.original_settings)};
            if (!restored) {
                message += "; restoring settings failed: " + restored.error();
            }
        }
        report_settings_error(message);
    }
    settings_changed.Broadcast();
    display_confirmation_changed.Broadcast(player_menu().prompt == OptionsPrompt::Display);
    if (!error.empty() || edit.action != host::PreferenceAction::PreviewDisplay) {
        queue_input_action(
            host::SettingsSaveResult{error.empty() && edit.action == host::PreferenceAction::Save});
    }
}

auto UOptionsDraft::apply_settings(GameSettingCategory category, GameSettingsState const& values)
    -> SettingsResult {
    if (category == GameSettingCategory::Video) {
        if (auto result{video_.apply_quality(values.video)}; !result) {
            return result;
        }
    }

    if (category == GameSettingCategory::Audio) {
        auto* audio{editing_local_player_->GetGameInstance()->GetSubsystem<UGameAudio>()};
        audio->set_master_volume(values.audio.master_volume);
        audio->set_music_volume(values.audio.music_volume);
        audio->set_sfx_volume(values.audio.sfx_volume);
        audio->set_ui_volume(values.audio.ui_volume);
    }

    return {};
}
void UOptionsDraft::confirm_display_changes() {
    if (player_menu().saving || player_menu().prompt != OptionsPrompt::Display) {
        return;
    }

    if (!save(GameSettingCategory::Video)) {
        revert_display_changes();
    }
}

void UOptionsDraft::revert_display_changes() {
    if (player_menu().saving || player_menu().prompt != OptionsPrompt::Display) {
        return;
    }

    player_menu().saving = true;
    queue_preferences({host::PreferenceAction::RevertDisplay,
                       GameSettingCategory::Video,
                       player_menu().draft.pending(),
                       player_menu().draft.applied()},
                      *this);
}

} // namespace ioj
