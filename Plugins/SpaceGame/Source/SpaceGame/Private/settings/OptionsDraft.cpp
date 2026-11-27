#include "SpaceGame/settings/OptionsDraft.h"

#include "SpaceGame/settings/ControlBindingView.h"
#include "SpaceGame/settings/GameSettingOption.h"
#include "SpaceGame/UnrealMessages.h"
#include <ioj/input/flight_controls.h>
#include <ioj/settings/game_setting.h>
#include <ioj/settings/game_setting_category.h>
#include <ioj/settings/game_setting_option_provider.h>
#include <ioj/settings/game_setting_value.h>
#include <ioj/settings/game_settings.h>
#include <ioj/settings/player_settings_view.h>
#include <SpaceGame/input/ControlBindingMetadata.h>
#include <SpaceGamePresentation/audio/GameAudio.h>

#include <SandboxCoreEngine/strings.h>

#include "Containers/Array.h"
#include "Containers/Ticker.h"
#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "HAL/Platform.h"
#include "HAL/PlatformTime.h"
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

#include <algorithm>
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
    if (save_pending_) {
        return;
    }

    check(!is_awaiting_display_confirmation());
    auto const& settings{player_settings()};
    editing_local_player_ = player;
    draft_.begin(settings.applied_values, settings.defaults);
    settings_changed.Broadcast();
}

void UOptionsDraft::cancel() {
    if (save_pending_) {
        return;
    }

    if (is_awaiting_display_confirmation()) {
        revert_display_changes();
    }

    draft_.cancel();
    settings_changed.Broadcast();
}

auto UOptionsDraft::apply(GameSettingCategory category) -> bool {
    if (save_pending_ || !editing_local_player_.IsValid() || is_awaiting_display_confirmation() ||
        !is_dirty()) {
        return false;
    }

    auto const& pending{draft_.pending()};
    auto const& applied{draft_.applied()};
    if (category == GameSettingCategory::Video &&
        (pending.video.window_mode != applied.video.window_mode ||
         pending.video.resolution != applied.video.resolution)) {
        save_pending_ = true;
        queue_preferences({host::PreferenceAction::PreviewDisplay, category, pending, applied},
                          *this);
        return true;
    }

    return save(category);
}

auto UOptionsDraft::save(GameSettingCategory category) -> bool {
    if (auto result{apply_settings(category, draft_.pending())}; !result) {
        report_settings_error("Applying settings failed: " + result.error());
        return false;
    }

    queue_preferences({host::PreferenceAction::Save, category, draft_.pending(), draft_.applied()},
                      *this);
    save_pending_ = true;
    return true;
}

void UOptionsDraft::preference_finished(host::PreferenceEdit const& edit, std::string_view error) {
    check(save_pending_);
    save_pending_ = false;
    auto const category{edit.category};
    if (edit.action != host::PreferenceAction::Save) {
        if (!error.empty()) {
            report_settings_error(std::string{error});
            save_completed.Broadcast(false);
            return;
        }

        if (edit.action == host::PreferenceAction::RevertDisplay) {
            draft_.cancel();
            finish_display_confirmation();
            save_completed.Broadcast(false);
        } else {
            confirmation_deadline_ = FPlatformTime::Seconds() + 15.0;
            display_confirmation_ticker_ = FTSTicker::GetCoreTicker().AddTicker(
                FTickerDelegate::CreateUObject(this, &ThisClass::tick_display_confirmation), 0.25f);
            display_confirmation_changed.Broadcast(true);
        }
        return;
    }

    if (!error.empty()) {
        std::string message{error};
        auto restored{apply_settings(category, draft_.applied())};

        report_settings_error("Saving settings failed: " + message +
                              (restored ? "" : "; restoring settings failed: " + restored.error()));
        if (is_awaiting_display_confirmation()) {
            finish_display_confirmation();
        }

        save_completed.Broadcast(false);
        return;
    }

    draft_.begin(player_settings().applied_values, player_settings().defaults);
    settings_changed.Broadcast();
    if (is_awaiting_display_confirmation()) {
        finish_display_confirmation();
    }

    save_completed.Broadcast(true);
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
void UOptionsDraft::reset_category(GameSettingCategory category) {
    if (!editing_local_player_.IsValid() || is_awaiting_display_confirmation()) {
        return;
    }

    draft_.reset_category(category);
    if (category == GameSettingCategory::Video) {
        if (player_settings().launch_mode != LaunchDisplayMode::Preferences) {
            draft_.set_setting(GameSetting::WindowMode, draft_.applied().video.window_mode);
        } else if (draft_.pending().video.window_mode == GameWindowMode::Fullscreen) {
            draft_.set_setting(GameSetting::WindowMode, GameWindowMode::Borderless);
        }

        if (!is_available(GameSetting::Resolution)) {
            draft_.set_setting(GameSetting::Resolution, draft_.applied().video.resolution);
        }
    }

    settings_changed.Broadcast();
}

void UOptionsDraft::confirm_display_changes() {
    if (save_pending_ || !is_awaiting_display_confirmation()) {
        return;
    }

    if (!save(GameSettingCategory::Video)) {
        revert_display_changes();
    }
}

void UOptionsDraft::revert_display_changes() {
    if (save_pending_ || !is_awaiting_display_confirmation()) {
        return;
    }

    save_pending_ = true;
    queue_preferences({host::PreferenceAction::RevertDisplay,
                       GameSettingCategory::Video,
                       draft_.pending(),
                       draft_.applied()},
                      *this);
}

auto UOptionsDraft::value(GameSetting const setting) const -> GameSettingValue {
    return draft_.value(setting);
}

void UOptionsDraft::set_setting(GameSetting const setting, GameSettingValue const& value) {
    if (!editing_local_player_.IsValid() || is_awaiting_display_confirmation() ||
        !is_available(setting)) {
        return;
    }

    auto const& descriptor{game_setting_descriptor(setting)};
    if (!player_settings().set_setting(draft_, setting, value, supported_resolutions())) {
        UE_LOG(LogTemp,
               Warning,
               TEXT("Rejected invalid value for game setting %s"),
               *to_fstring(descriptor.name));
        return;
    }

    settings_changed.Broadcast();
}

auto UOptionsDraft::descriptors(GameSettingCategory const category) const
    -> TArray<GameSettingDescriptor const*> {
    TArray<GameSettingDescriptor const*> result;
    for (auto const& descriptor : game_setting_descriptors()) {
        if (descriptor.category == category) {
            result.Add(&descriptor);
        }
    }

    return result;
}

auto UOptionsDraft::options(GameSetting const setting) const -> TArray<FGameSettingOption> {
    auto const provider{game_setting_descriptor(setting).options_provider};
    if (!editing_local_player_.IsValid()) {
        return {};
    }

    auto const choices{
        player_settings().choices(draft_.pending(), provider, supported_resolutions())};
    TArray<FGameSettingOption> result;
    result.Reserve(static_cast<int32>(choices.size()));
    for (auto const& choice : choices) {
        auto const* number{std::get_if<float>(&choice.value)};
        auto label{provider == GameSettingOptionProvider::FrameRateLimits && number && *number > 0.f
                       ? FText::AsNumber(static_cast<int32>(*number))
                       : FText::FromString(to_fstring(choice.label))};
        result.Add({choice.value, MoveTemp(label)});
    }

    return result;
}

auto UOptionsDraft::is_available(GameSetting const setting) const -> bool {
    return editing_local_player_.IsValid() &&
           player_settings().is_available(draft_.pending(), setting);
}

auto UOptionsDraft::is_dirty() const -> bool {
    return draft_.is_dirty();
}

auto UOptionsDraft::is_at_defaults(GameSettingCategory const category) const -> bool {
    return draft_.is_at_defaults(category);
}

auto UOptionsDraft::is_awaiting_display_confirmation() const -> bool {
    return confirmation_deadline_ != 0.0;
}

auto UOptionsDraft::display_confirmation_seconds_remaining() const -> int32 {
    return FMath::Max(0, FMath::CeilToInt(confirmation_deadline_ - FPlatformTime::Seconds()));
}

/* **************************************** */
// Settings and display helpers
/* **************************************** */
void UOptionsDraft::finish_display_confirmation() {
    confirmation_deadline_ = 0.0;
    if (display_confirmation_ticker_.IsValid()) {
        FTSTicker::GetCoreTicker().RemoveTicker(display_confirmation_ticker_);
        display_confirmation_ticker_.Reset();
    }

    display_confirmation_changed.Broadcast(false);
    settings_changed.Broadcast();
}

auto UOptionsDraft::tick_display_confirmation(float const delta_seconds) -> bool {
    static_cast<void>(delta_seconds);
    if (!is_awaiting_display_confirmation()) {
        return false;
    }

    if (FPlatformTime::Seconds() >= confirmation_deadline_) {
        revert_display_changes();
        return is_awaiting_display_confirmation();
    }

    display_confirmation_changed.Broadcast(true);
    return true;
}

/* **************************************** */
// Control bindings
/* **************************************** */

auto UOptionsDraft::control_bindings(GameSettingDevice device) const
    -> TArray<FControlBindingView> {
    TArray<FControlBindingView> result;
    for (auto const& control : flight_controls()) {
        if (is_gamepad_mapping(control.mapping) == (device == GameSettingDevice::Controller)) {
            result.Add(FControlBindingView{control.mapping,
                                           FText::FromString(to_fstring(control.label)),
                                           control_binding_group_label(control.group),
                                           draft_.pending().controls.bindings[control.mapping],
                                           draft_.defaults().controls.bindings[control.mapping]});
        }
    }
    return result;
}
auto UOptionsDraft::binding_conflicts(FlightMapping mapping, PhysicalKey key) const
    -> TArray<FText> {
    TArray<FText> result;
    auto const conflicts{draft_.binding_conflicts(mapping, key)};
    for (auto const& control : flight_controls()) {
        if (std::ranges::contains(conflicts, control.mapping)) {
            result.Add(FText::FromString(to_fstring(control.label)));
        }
    }
    return result;
}
auto UOptionsDraft::set_control_binding(FlightMapping mapping,
                                        PhysicalKey key,
                                        bool replace_conflicts) -> bool {
    if (!draft_.set_binding(mapping, key, replace_conflicts)) {
        return false;
    }
    settings_changed.Broadcast();
    return true;
}
}
