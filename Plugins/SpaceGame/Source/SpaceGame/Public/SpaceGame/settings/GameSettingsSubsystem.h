#pragma once

#include "ioj/settings/game_settings_edit_state.h"
#include "SpaceGame/settings/GameSettingsBackend.h"
#include <ioj/settings/game_settings.h>
#include <ioj/settings/game_settings_access.h>
#include <ioj/sim/player/game_settings_session.h>
#include <SpaceGame/settings/ControlBindings.h>

#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "GameSettingsSubsystem.generated.h"

class ULocalPlayer;

namespace ioj {

class USpaceGameInputUserSettings;

DECLARE_MULTICAST_DELEGATE(FGameSettingsChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(FGameSettingChanged, GameSetting);
DECLARE_MULTICAST_DELEGATE(FGameFlightModelConfigChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(FDisplayConfirmationChanged, bool);

UCLASS()
class SPACEGAME_API UGameSettingsSubsystem final
    : public UGameInstanceSubsystem
    , public GameSettingsAccess<UGameSettingsSubsystem> {
    GENERATED_BODY()
  public:
    /* **************************************** */
    // Lifecycle and edit sessions
    /* **************************************** */
    virtual void Initialize(FSubsystemCollectionBase& collection) override;
    virtual void Deinitialize() override;

    void begin_edit(ULocalPlayer* local_player);
    void cancel();
    void apply();
    void reset_category(GameSettingCategory category);
    void confirm_display_changes();
    void revert_display_changes();

    /* **************************************** */
    // Settings state
    /* **************************************** */
    auto settings_state() const -> GameSettingsState const&;
    auto applied_state() const -> GameSettingsState const&;
    auto default_state() const -> GameSettingsState const&;
    auto value(GameSetting setting) const -> GameSettingValue;
    void set_setting(GameSetting setting, GameSettingValue const& value);
    auto descriptors(GameSettingCategory category) const -> TArray<GameSettingDescriptor const*>;
    auto options(GameSetting setting) const -> TArray<FGameSettingOption>;
    auto is_available(GameSetting setting) const -> bool;
    auto is_dirty() const -> bool;
    auto is_dirty(GameSettingCategory category) const -> bool;
    auto is_at_defaults(GameSettingCategory category) const -> bool;
    auto flight_model_profile(EShipControlScope scope) const
        -> sim::player::FlightModelProfile const&;
    auto flight_model_loadout() const -> sim::player::FlightModelLoadout const&;
    auto set_flight_model_profile(EShipControlScope scope, sim::player::FlightModelProfile profile)
        -> bool;
    auto is_awaiting_display_confirmation() const -> bool;
    auto display_confirmation_seconds_remaining() const -> int32;

    /* **************************************** */
    // Control bindings
    /* **************************************** */
    auto control_bindings(EHardwareDevicePrimaryType device_type) const
        -> TArray<FControlBindingView>;
    auto control_bindings(EHardwareDevicePrimaryType device_type, EShipControlScope scope) const
        -> TArray<FControlBindingView>;
    auto binding_conflicts(FControlBindingAddress const& address, FKey key) const
        -> TArray<FControlBindingView>;
    auto chord_binding_conflicts(FControlBindingAddress const& address,
                                 FKey activator_key,
                                 FKey action_key) const -> TArray<FControlBindingView>;
    auto set_control_binding(FControlBindingAddress const& address,
                             FKey key,
                             bool replace_conflicts) -> bool;
    auto set_control_chord(FControlBindingAddress const& address,
                           FKey activator_key,
                           FKey action_key,
                           bool replace_conflicts) -> bool;
    auto clear_control_binding(FControlBindingAddress const& address) -> bool;
    auto reset_control_binding(FControlBindingAddress const& address) -> bool;
    auto reset_all_control_bindings() -> bool;

    FGameSettingsChanged settings_changed;
    FGameSettingChanged setting_changed;
    FGameFlightModelConfigChanged flight_model_config_changed;
    FDisplayConfirmationChanged display_confirmation_changed;
  private:
    /* **************************************** */
    // Settings and display helpers
    /* **************************************** */
    auto tick_display_confirmation(float delta_seconds) -> bool;
    void preview_immediate_settings(GameSettingsState const& state);
    auto normalize_value(GameSettingDescriptor const& descriptor,
                         GameSettingValue const& value) const -> std::optional<GameSettingValue>;
    auto input_user_settings() const -> USpaceGameInputUserSettings*;

    /* **************************************** */
    // Control binding helpers
    /* **************************************** */
    auto all_control_bindings() const -> TArray<FControlBindingView>;
    auto map_control_binding(USpaceGameInputUserSettings& settings,
                             FControlBindingAddress const& address,
                             FKey key,
                             bool defer_change_broadcast) const -> bool;
    auto unmap_control_binding(USpaceGameInputUserSettings& settings,
                               FControlBindingAddress const& address) const -> bool;
    void capture_input_edit_state();
    void restore_input_edit_state();
    auto input_is_dirty() const -> bool;

    /* **************************************** */
    // State
    /* **************************************** */
    FGameSettingsBackend backend_;
    TWeakObjectPtr<ULocalPlayer> editing_local_player_{};
    GameSettingsSession session_;
    std::vector<ControlBindingSnapshot> applied_control_bindings_{};
    FTSTicker::FDelegateHandle display_confirmation_ticker_;
};

} // namespace ioj
