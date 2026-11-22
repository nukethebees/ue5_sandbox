#include "SpaceGame/settings/GameSettingsSubsystem.h"
#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
#include "EmbeddedHost.h"
#endif

#include "SpaceGame/input/CanonicalShipControls.h"
#include "SpaceGame/input/SpaceGameInputUserSettings.h"
#include "SpaceGame/settings/SpaceGameUserSettings.h"
#include <ioj/levels/authoring/game_configuration.h>

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "HAL/PlatformTime.h"
#include <Engine/GameInstance.h>
#include <HAL/PlatformProcess.h>
#include <Misc/App.h>
#include <Misc/MessageDialog.h>
#include <Misc/Paths.h>

namespace ioj {
namespace {
#if !defined(UE_LIBRARY_ENABLED) || !UE_LIBRARY_ENABLED
auto script_root() -> std::filesystem::path {
    return std::filesystem::path{*FPaths::ConvertRelativePathToFull(FPaths::ProjectDir())} /
           "LevelScripts";
}
#endif
auto personal_settings_path() -> std::filesystem::path {
#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
    return embedded_game_host().preferences_path;
#elif WITH_EDITOR
    return script_root() / "Settings/user-settings.scm";
#else
    auto const local{FPlatformProcess::GetApplicationSettingsDir(
        {FPlatformProcess::ApplicationSettingsContext::Context::LocalUser, false})};
    return std::filesystem::path{*local} / "IsleOfJebbers/Config/user-settings.scm";
#endif
}
auto load_preferences() -> levels::authoring::UserSettingsResult {
#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
    auto const& session{embedded_game_host().session};
    return levels::authoring::UserSettings{session.default_state(), session.applied_state()};
#else
    auto const desktop{GEngine->GetGameUserSettings()->GetDesktopResolution()};
    return levels::authoring::read_user_settings(
        script_root(),
        personal_settings_path(),
        {desktop.X > 0 ? desktop.X : 1920, desktop.Y > 0 ? desktop.Y : 1080});
#endif
}
void report_settings_error(std::string const& error, bool interactive) {
    auto const message{FText::FromString(UTF8_TO_TCHAR(error.c_str()))};
    UE_LOG(LogTemp, Error, TEXT("%s"), *message.ToString());
    if (interactive && !FApp::IsUnattended() && !GIsAutomationTesting) {
        FMessageDialog::Open(EAppMsgType::Ok, message);
    }
}
auto persist(GameSettingsState const& state, GameSettingsState const& defaults) -> bool {
    auto result{levels::authoring::save_user_settings(personal_settings_path(), state, defaults)};
    if (!result) {
        report_settings_error(result.error(), true);
    }
    return result.has_value();
}
}

/* **************************************** */
// Lifecycle
/* **************************************** */
void UGameSettingsSubsystem::Initialize(FSubsystemCollectionBase& collection) {
    Super::Initialize(collection);
#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
    application_ = embedded_game_host().application;
    backend_.apply_non_display(session().applied_state());
#else
    auto app{levels::authoring::read_application_settings(script_root())};
    if (!app) {
        UE_LOG(LogTemp, Fatal, TEXT("%s"), UTF8_TO_TCHAR(app.error().c_str()));
        return;
    }
    application_ = *app;
    auto config{load_preferences()};
    if (!config) {
        report_settings_error(config.error(), false);
        return;
    }
    session().initialise(config->applied, config->defaults);
    backend_.apply_non_display(config->applied);
    backend_.apply_display(config->applied);
    backend_.confirm_display();
#endif
}

auto UGameSettingsSubsystem::session() -> GameSettingsSession& {
#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
    return embedded_game_host().session;
#else
    return session_;
#endif
}
auto UGameSettingsSubsystem::session() const -> GameSettingsSession const& {
#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
    return embedded_game_host().session;
#else
    return session_;
#endif
}

void UGameSettingsSubsystem::Deinitialize() {
    if (session().is_awaiting_display_confirmation()) {
        revert_display_changes();
    }
    Super::Deinitialize();
}

/* **************************************** */
// Edit sessions and display confirmation
/* **************************************** */
void UGameSettingsSubsystem::begin_edit(ULocalPlayer* const local_player) {
    if (session().is_awaiting_display_confirmation()) {
        return;
    }
    editing_local_player_ = local_player;
    auto config{load_preferences()};
    if (!config) {
        cancel();
        report_settings_error(config.error(), true);
        return;
    }
    if (!session().begin_edit(config->applied, config->defaults)) {
        return;
    }
    backend_.apply_non_display(config->applied);
    backend_.apply_display(config->applied);
    backend_.confirm_display();
    capture_input_edit_state();
    settings_changed.Broadcast();
}

void UGameSettingsSubsystem::cancel() {
    if (!session().is_editing() || session_.is_awaiting_display_confirmation()) {
        return;
    }
    auto const before{session().settings_state()};
    restore_input_edit_state();
    session().cancel();
    for (auto const& descriptor : game_setting_descriptors()) {
        if (game_setting_value(before, descriptor.id) != session().value(descriptor.id)) {
            setting_changed.Broadcast(descriptor.id);
        }
    }
    settings_changed.Broadcast();
}

void UGameSettingsSubsystem::apply() {
    if (!session().is_editing() || session_.is_awaiting_display_confirmation() || !is_dirty()) {
        return;
    }
    auto const& pending{session().settings_state()};
    auto const bindings_changed{input_is_dirty()};
    auto next{session()};
    auto const result{next.apply(FPlatformTime::Seconds(), bindings_changed)};
    if (next.applied_state() != session().applied_state() &&
        !persist(next.applied_state(), next.default_state())) {
        return;
    }
    backend_.apply_non_display(pending);
    if (bindings_changed) {
        if (auto* const input_settings{input_user_settings()}) {
            input_settings->ApplySettings();
            input_settings->AsyncSaveSettings();
        }
    }
    session() = std::move(next);
    capture_input_edit_state();
    if (result == GameSettingsApplyResult::Applied) {
        settings_changed.Broadcast();
        return;
    }
    backend_.apply_display(session().settings_state());
    display_confirmation_ticker_ = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &ThisClass::tick_display_confirmation), 0.25f);
    display_confirmation_changed.Broadcast(true);
    settings_changed.Broadcast();
}

void UGameSettingsSubsystem::reset_category(GameSettingCategory const category) {
    auto const before{session().settings_state()};
    if (!session().reset_category(category)) {
        return;
    }
    if (category == GameSettingCategory::Controls) {
        reset_all_control_bindings();
    }
    for (auto const& descriptor : game_setting_descriptors()) {
        if (descriptor.category == category &&
            descriptor.apply_mode == SettingApplyMode::Immediate &&
            game_setting_value(before, descriptor.id) != session().value(descriptor.id)) {
            setting_changed.Broadcast(descriptor.id);
        }
    }
    settings_changed.Broadcast();
}

void UGameSettingsSubsystem::confirm_display_changes() {
    if (!session().is_awaiting_display_confirmation() ||
        !persist(session().settings_state(), session_.default_state()) ||
        !session().confirm_display()) {
        return;
    }
    backend_.confirm_display();
    if (display_confirmation_ticker_.IsValid()) {
        FTSTicker::GetCoreTicker().RemoveTicker(display_confirmation_ticker_);
        display_confirmation_ticker_.Reset();
    }
    display_confirmation_changed.Broadcast(false);
    settings_changed.Broadcast();
}

void UGameSettingsSubsystem::revert_display_changes() {
    if (!session().revert_display()) {
        return;
    }
    backend_.revert_display();
    if (display_confirmation_ticker_.IsValid()) {
        FTSTicker::GetCoreTicker().RemoveTicker(display_confirmation_ticker_);
        display_confirmation_ticker_.Reset();
    }
    display_confirmation_changed.Broadcast(false);
    settings_changed.Broadcast();
}

/* **************************************** */
// Settings state
/* **************************************** */
auto UGameSettingsSubsystem::application_settings(UGameInstance const* instance)
    -> ApplicationSettings const& {
    auto const* settings{UGameInstance::GetSubsystem<UGameSettingsSubsystem>(instance)};
    check(settings);
    return settings->application_;
}

auto UGameSettingsSubsystem::settings_state() const -> GameSettingsState const& {
    return session().settings_state();
}

auto UGameSettingsSubsystem::applied_state() const -> GameSettingsState const& {
    return session().applied_state();
}

auto UGameSettingsSubsystem::default_state() const -> GameSettingsState const& {
    return session().default_state();
}

auto UGameSettingsSubsystem::value(GameSetting const setting) const -> GameSettingValue {
    return session().value(setting);
}

void UGameSettingsSubsystem::set_setting(GameSetting const setting, GameSettingValue const& value) {
    if (!session().is_editing() || session_.is_awaiting_display_confirmation()) {
        return;
    }
    auto const& descriptor{game_setting_descriptor(setting)};
    auto normalized{normalize_value(descriptor, value)};
    if (!normalized || !session().set_setting(setting, *normalized)) {
        UE_LOG(LogTemp,
               Warning,
               TEXT("Rejected invalid value for game setting %s"),
               UTF8_TO_TCHAR(descriptor.name));
        return;
    }
    setting_changed.Broadcast(setting);
    settings_changed.Broadcast();
}

auto UGameSettingsSubsystem::descriptors(GameSettingCategory const category) const
    -> TArray<GameSettingDescriptor const*> {
    TArray<GameSettingDescriptor const*> result;
    for (auto const& descriptor : game_setting_descriptors()) {
        if (descriptor.category == category) {
            result.Add(&descriptor);
        }
    }
    return result;
}

auto UGameSettingsSubsystem::options(GameSetting const setting) const
    -> TArray<FGameSettingOption> {
    return backend_.options(game_setting_descriptor(setting).options_provider);
}

auto UGameSettingsSubsystem::is_available(GameSetting const setting) const -> bool {
    return game_setting_is_available(game_setting_descriptor(setting).availability_provider,
                                     session().settings_state());
}

auto UGameSettingsSubsystem::is_dirty() const -> bool {
    return session().is_dirty() || input_is_dirty();
}

auto UGameSettingsSubsystem::is_dirty(GameSettingCategory const category) const -> bool {
    return session().is_dirty(category) ||
           (category == GameSettingCategory::Controls && input_is_dirty());
}

auto UGameSettingsSubsystem::is_at_defaults(GameSettingCategory const category) const -> bool {
    if (category == GameSettingCategory::Controls) {
        auto const bindings{control_bindings(EHardwareDevicePrimaryType::Unspecified)};
        if (bindings.ContainsByPredicate(
                [](auto const& binding) { return binding.is_modified(); })) {
            return false;
        }
    }
    return session().is_at_defaults(category);
}

auto UGameSettingsSubsystem::is_awaiting_display_confirmation() const -> bool {
    return session().is_awaiting_display_confirmation();
}

auto UGameSettingsSubsystem::display_confirmation_seconds_remaining() const -> int32 {
    return session().display_confirmation_seconds_remaining(FPlatformTime::Seconds());
}

/* **************************************** */
// Settings and display helpers
/* **************************************** */
auto UGameSettingsSubsystem::tick_display_confirmation(float const delta_seconds) -> bool {
    static_cast<void>(delta_seconds);
    if (!session().is_awaiting_display_confirmation()) {
        return false;
    }
    if (session().display_confirmation_expired(FPlatformTime::Seconds())) {
        revert_display_changes();
        return false;
    }
    display_confirmation_changed.Broadcast(true);
    return true;
}

auto UGameSettingsSubsystem::normalize_value(GameSettingDescriptor const& descriptor,
                                             GameSettingValue const& value) const
    -> std::optional<GameSettingValue> {
    std::vector<GameSettingValue> choices;
    if (descriptor.control_kind == SettingControlKind::Choice) {
        for (auto const& option : backend_.options(descriptor.options_provider)) {
            choices.push_back(option.value);
        }
    }
    return normalize_game_setting_value(descriptor, value, choices, true);
}

auto UGameSettingsSubsystem::input_user_settings() const -> USpaceGameInputUserSettings* {
    auto* const local_player{editing_local_player_.Get()};
    auto* const subsystem{
        local_player != nullptr
            ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(local_player)
            : nullptr};
    return subsystem != nullptr ? Cast<USpaceGameInputUserSettings>(subsystem->GetUserSettings())
                                : nullptr;
}

/* **************************************** */
// Control bindings
/* **************************************** */

auto UGameSettingsSubsystem::all_control_bindings() const -> TArray<FControlBindingView> {
    TArray<FControlBindingView> result;

    auto const* const settings{input_user_settings()};
    if (settings == nullptr) {
        return result;
    }

    for (auto const& profile_pair : settings->GetAllAvailableKeyProfiles()) {
        auto const* const profile{profile_pair.Value.Get()};
        if (!IsValid(profile)) {
            continue;
        }
        for (auto const& row : profile->GetPlayerMappingRows()) {
            for (auto const& mapping : row.Value.Mappings) {
                auto const* const presentation{
                    settings->control_binding_metadata(profile_pair.Key, mapping)};
                if (presentation == nullptr) {
                    continue;
                }
                TOptional<FControlChordBindingView> chord;
                if (auto const* const chord_mapping{
                        settings->chord_mapping_for_mapping(profile_pair.Key, mapping)}) {
                    chord = FControlChordBindingView{
                        .address =
                            {
                                .profile_id = profile_pair.Key,
                                .mapping_name = chord_mapping->GetMappingName(),
                                .hardware_device_id =
                                    chord_mapping->GetHardwareDeviceId().HardwareDeviceIdentifier,
                                .slot = chord_mapping->GetSlot(),
                            },
                        .current_key = chord_mapping->GetCurrentKey(),
                        .default_key = chord_mapping->GetDefaultKey(),
                    };
                }

                result.Add(FControlBindingView{
                    .address =
                        {
                            .profile_id = profile_pair.Key,
                            .mapping_name = mapping.GetMappingName(),
                            .hardware_device_id =
                                mapping.GetHardwareDeviceId().HardwareDeviceIdentifier,
                            .slot = mapping.GetSlot(),
                        },
                    .display_name = mapping.GetDisplayName(),
                    .display_category = mapping.GetDisplayCategory(),
                    .display_group = presentation->group,
                    .display_order = presentation->display_order,
                    .device_type = mapping.GetPrimaryDeviceType(),
                    .current_key = mapping.GetCurrentKey(),
                    .default_key = mapping.GetDefaultKey(),
                    .chord = MoveTemp(chord),
                });
            }
        }
    }

    result.Sort([](auto const& left, auto const& right) {
        if (left.address.profile_id != right.address.profile_id) {
            return left.address.profile_id < right.address.profile_id;
        }
        if (left.display_group != right.display_group) {
            return static_cast<uint8>(left.display_group) < static_cast<uint8>(right.display_group);
        }
        if (left.display_order != right.display_order) {
            return left.display_order < right.display_order;
        }
        if (left.device_type != right.device_type) {
            return static_cast<uint8>(left.device_type) < static_cast<uint8>(right.device_type);
        }
        if (left.address.mapping_name != right.address.mapping_name) {
            return left.address.mapping_name.LexicalLess(right.address.mapping_name);
        }
        return static_cast<uint8>(left.address.slot) < static_cast<uint8>(right.address.slot);
    });

    return result;
}

auto UGameSettingsSubsystem::control_bindings(EHardwareDevicePrimaryType const device_type) const
    -> TArray<FControlBindingView> {
    auto* const settings{input_user_settings()};
    if (settings == nullptr) {
        return {};
    }

    auto bindings{all_control_bindings().FilterByPredicate([settings](auto const& binding) {
        return binding.address.profile_id == settings->GetActiveKeyProfileId();
    })};

    return bindings.FilterByPredicate([device_type](auto const& binding) {
        return control_binding_matches_device(binding, device_type);
    });
}

auto UGameSettingsSubsystem::binding_conflicts(FControlBindingAddress const& address,
                                               FKey const key) const
    -> TArray<FControlBindingView> {
    if (!key.IsValid()) {
        return {};
    }
    auto const bindings{control_bindings(EHardwareDevicePrimaryType::Unspecified)};
    auto const native{native_control_bindings(bindings)};
    TArray<FControlBindingView> result;
    for (auto const index :
         control_binding_conflicts(native,
                                   native_control_binding_address(address),
                                   TCHAR_TO_UTF8(*key.GetFName().ToString().ToLower()))) {
        result.Add(bindings[static_cast<int32>(index)]);
    }
    return result;
}

auto UGameSettingsSubsystem::chord_binding_conflicts(FControlBindingAddress const& address,
                                                     FKey const activator_key,
                                                     FKey const action_key) const
    -> TArray<FControlBindingView> {
    if (!activator_key.IsValid() || !action_key.IsValid()) {
        return {};
    }
    auto const bindings{control_bindings(EHardwareDevicePrimaryType::Unspecified)};
    auto const native{native_control_bindings(bindings)};
    TArray<FControlBindingView> result;
    for (auto const index : control_chord_binding_conflicts(
             native,
             native_control_binding_address(address),
             TCHAR_TO_UTF8(*activator_key.GetFName().ToString().ToLower()),
             TCHAR_TO_UTF8(*action_key.GetFName().ToString().ToLower()))) {
        result.Add(bindings[static_cast<int32>(index)]);
    }
    return result;
}

auto UGameSettingsSubsystem::map_control_binding(USpaceGameInputUserSettings& settings,
                                                 FControlBindingAddress const& address,
                                                 FKey const key,
                                                 bool const defer_change_broadcast) const -> bool {
    FMapPlayerKeyArgs arguments{};
    arguments.MappingName = address.mapping_name;
    arguments.Slot = address.slot;
    arguments.NewKey = key;
    arguments.HardwareDeviceId = address.hardware_device_id;
    arguments.ProfileIdString = address.profile_id;
    arguments.bDeferOnSettingsChangedBroadcast = defer_change_broadcast;

    FGameplayTagContainer failure_reason;
    settings.MapPlayerKey(arguments, failure_reason);
    if (!failure_reason.IsEmpty()) {
        UE_LOG(LogTemp,
               Warning,
               TEXT("Could not map control '%s': %s"),
               *address.mapping_name.ToString(),
               *failure_reason.ToStringSimple());
        return false;
    }

    return true;
}

auto UGameSettingsSubsystem::unmap_control_binding(USpaceGameInputUserSettings& settings,
                                                   FControlBindingAddress const& address) const
    -> bool {
    FMapPlayerKeyArgs arguments{};
    arguments.MappingName = address.mapping_name;
    arguments.Slot = address.slot;
    arguments.HardwareDeviceId = address.hardware_device_id;
    arguments.ProfileIdString = address.profile_id;
    FGameplayTagContainer failure_reason;
    settings.UnMapPlayerKey(arguments, failure_reason);
    if (!failure_reason.IsEmpty()) {
        UE_LOG(LogTemp,
               Warning,
               TEXT("Could not reset control '%s': %s"),
               *address.mapping_name.ToString(),
               *failure_reason.ToStringSimple());
        return false;
    }
    return true;
}

auto UGameSettingsSubsystem::set_control_binding(FControlBindingAddress const& address,
                                                 FKey const key,
                                                 bool const replace_conflicts) -> bool {
    auto* const settings{input_user_settings()};
    if (!session().is_editing() || settings == nullptr || !key.IsValid() ||
        address.profile_id != settings->GetActiveKeyProfileId()) {
        return false;
    }

    auto const bindings{control_bindings(EHardwareDevicePrimaryType::Unspecified)};
    auto const* const target{bindings.FindByPredicate(
        [&address](auto const& binding) { return binding.address == address; })};
    if (target == nullptr ||
        (target->device_type == EHardwareDevicePrimaryType::Gamepad) != key.IsGamepadKey()) {
        return false;
    }

    auto const conflicts{binding_conflicts(address, key)};
    if (!conflicts.IsEmpty() && !replace_conflicts) {
        return false;
    }

    TArray<FControlBindingView> unmapped_conflicts;
    unmapped_conflicts.Reserve(conflicts.Num());
    for (auto const& conflict : conflicts) {
        if (!map_control_binding(*settings, conflict.address, EKeys::Invalid, false)) {
            for (auto const& unmapped : unmapped_conflicts) {
                static_cast<void>(
                    map_control_binding(*settings, unmapped.address, unmapped.current_key, false));
            }
            return false;
        }
        unmapped_conflicts.Add(conflict);
    }

    if (!map_control_binding(*settings, address, key, false)) {
        for (auto const& unmapped : unmapped_conflicts) {
            static_cast<void>(
                map_control_binding(*settings, unmapped.address, unmapped.current_key, false));
        }
        return false;
    }

    settings_changed.Broadcast();
    return true;
}

auto UGameSettingsSubsystem::set_control_chord(FControlBindingAddress const& address,
                                               FKey const activator_key,
                                               FKey const action_key,
                                               bool const replace_conflicts) -> bool {
    auto* const settings{input_user_settings()};
    if (!session().is_editing() || settings == nullptr || !can_hold_chord_key(activator_key) ||
        !action_key.IsValid() || activator_key == action_key ||
        address.profile_id != settings->GetActiveKeyProfileId()) {
        return false;
    }

    auto const bindings{control_bindings(EHardwareDevicePrimaryType::Unspecified)};
    auto const* const target{bindings.FindByPredicate(
        [&address](auto const& binding) { return binding.address == address; })};
    if (target == nullptr || !target->chord.IsSet()) {
        return false;
    }
    auto const gamepad{target->device_type == EHardwareDevicePrimaryType::Gamepad};
    if (activator_key.IsGamepadKey() != gamepad || action_key.IsGamepadKey() != gamepad) {
        return false;
    }

    auto const conflicts{chord_binding_conflicts(address, activator_key, action_key)};
    if (!conflicts.IsEmpty() && !replace_conflicts) {
        return false;
    }

    TArray<FControlBindingView> changed;
    changed.Reserve(conflicts.Num() + 2);
    for (auto const& conflict : conflicts) {
        if (!map_control_binding(*settings, conflict.address, EKeys::Invalid, true)) {
            for (auto const& original : changed) {
                static_cast<void>(
                    map_control_binding(*settings, original.address, original.current_key, true));
            }
            return false;
        }
        changed.Add(conflict);
    }

    auto apply_target = [this, settings, &changed](FControlBindingAddress const& target_address,
                                                   FKey const old_key,
                                                   FKey const new_key) {
        if (old_key == new_key) {
            return true;
        }
        if (!map_control_binding(*settings, target_address, new_key, true)) {
            for (auto const& original : changed) {
                static_cast<void>(
                    map_control_binding(*settings, original.address, original.current_key, true));
            }
            return false;
        }
        changed.Add(FControlBindingView{.address = target_address, .current_key = old_key});
        return true;
    };
    if (!apply_target(target->chord->address, target->chord->current_key, activator_key) ||
        !apply_target(address, target->current_key, action_key)) {
        return false;
    }

    settings_changed.Broadcast();
    return true;
}

auto UGameSettingsSubsystem::clear_control_binding(FControlBindingAddress const& address) -> bool {
    auto* const settings{input_user_settings()};
    if (!session().is_editing() || settings == nullptr ||
        address.profile_id != settings->GetActiveKeyProfileId()) {
        return false;
    }
    auto const bindings{control_bindings(EHardwareDevicePrimaryType::Unspecified)};
    auto const* const target{bindings.FindByPredicate(
        [&address](auto const& binding) { return binding.address == address; })};
    if (target == nullptr || !target->current_key.IsValid()) {
        return false;
    }

    FMapPlayerKeyArgs arguments{};
    arguments.MappingName = address.mapping_name;
    arguments.Slot = address.slot;
    arguments.NewKey = EKeys::Invalid;
    arguments.HardwareDeviceId = address.hardware_device_id;
    arguments.ProfileIdString = address.profile_id;
    FGameplayTagContainer failure_reason;
    settings->MapPlayerKey(arguments, failure_reason);
    if (!failure_reason.IsEmpty()) {
        UE_LOG(LogTemp,
               Warning,
               TEXT("Could not clear control '%s': %s"),
               *address.mapping_name.ToString(),
               *failure_reason.ToStringSimple());
        return false;
    }
    settings_changed.Broadcast();
    return true;
}

auto UGameSettingsSubsystem::reset_control_binding(FControlBindingAddress const& address) -> bool {
    auto* const settings{input_user_settings()};
    if (!session().is_editing() || settings == nullptr) {
        return false;
    }
    if (!unmap_control_binding(*settings, address)) {
        return false;
    }
    settings_changed.Broadcast();
    return true;
}

auto UGameSettingsSubsystem::reset_all_control_bindings() -> bool {
    auto* const settings{input_user_settings()};
    if (!session().is_editing() || settings == nullptr) {
        return false;
    }
    auto const bindings{control_bindings(EHardwareDevicePrimaryType::Unspecified)};
    for (auto const& binding : bindings) {
        if (!unmap_control_binding(*settings, binding.address)) {
            return false;
        }
    }
    settings_changed.Broadcast();
    return true;
}

/* **************************************** */
// Input edit state
/* **************************************** */
void UGameSettingsSubsystem::capture_input_edit_state() {
    applied_control_bindings_ = native_control_bindings(all_control_bindings());
}

void UGameSettingsSubsystem::restore_input_edit_state() {
    auto* const settings{input_user_settings()};
    if (settings == nullptr) {
        return;
    }

    for (auto const& binding : applied_control_bindings_) {
        if (binding.current_key == binding.default_key) {
            static_cast<void>(
                unmap_control_binding(*settings, engine_control_binding_address(binding.address)));
        } else {
            static_cast<void>(
                map_control_binding(*settings,
                                    engine_control_binding_address(binding.address),
                                    FKey{FName{UTF8_TO_TCHAR(binding.current_key.c_str())}},
                                    true));
        }
    }
    settings->ApplySettings();
}

auto UGameSettingsSubsystem::input_is_dirty() const -> bool {
    return control_bindings_changed(native_control_bindings(all_control_bindings()),
                                    applied_control_bindings_);
}

} // namespace ioj
