#pragma once

#include "Containers/Ticker.h"
#include "SpaceGame/settings/ControlSettingsTypes.h"
#include "SpaceGame/settings/GameSettings.generated.h"
#include "SpaceGame/settings/GameSettingsBackend.h"
#include "SpaceGame/settings/GameSettingsEditState.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "GameSettingsSubsystem.generated.h"

class ULocalPlayer;

namespace ml::ioj {

class USpaceGameInputUserSettings;

DECLARE_MULTICAST_DELEGATE(FGameSettingsChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(FDisplayConfirmationChanged, bool);

UCLASS()
class SPACEGAME_API UGameSettingsSubsystem final
    : public UGameInstanceSubsystem
    , public TGameSettingsAccess<UGameSettingsSubsystem> {
    GENERATED_BODY()
  public:
    virtual void Initialize(FSubsystemCollectionBase& collection) override;
    virtual void Deinitialize() override;

    void begin_edit(ULocalPlayer* local_player);
    void cancel();
    void apply();
    void reset_category(EGameSettingCategory category);
    void confirm_display_changes();
    void revert_display_changes();

    auto settings_state() const -> FGameSettingsState const&;
    auto applied_state() const -> FGameSettingsState const&;
    auto default_state() const -> FGameSettingsState const&;
    auto value(EGameSetting setting) const -> FGameSettingValue;
    void set_setting(EGameSetting setting, FGameSettingValue const& value);
    auto descriptors(EGameSettingCategory category) const -> TArray<FGameSettingDescriptor const*>;
    auto options(EGameSetting setting) const -> TArray<FGameSettingOption>;
    auto is_available(EGameSetting setting) const -> bool;
    auto is_dirty() const -> bool;
    auto is_dirty(EGameSettingCategory category) const -> bool;
    auto is_at_defaults(EGameSettingCategory category) const -> bool;
    auto is_awaiting_display_confirmation() const -> bool;
    auto display_confirmation_seconds_remaining() const -> int32;

    auto control_profiles() const -> TArray<FControlProfileView>;
    auto control_bindings(EHardwareDevicePrimaryType device_type) const
        -> TArray<FControlBindingView>;
    auto binding_conflicts(FControlBindingAddress const& address, FKey key) const
        -> TArray<FControlBindingView>;
    auto set_control_profile(FString const& profile_id) -> bool;
    auto create_custom_control_profile() -> bool;
    auto rename_active_custom_control_profile(FString const& display_name) -> bool;
    auto delete_active_custom_control_profile() -> bool;
    auto set_control_binding(FControlBindingAddress const& address,
                             FKey key,
                             bool replace_conflicts) -> bool;
    auto reset_control_binding(FControlBindingAddress const& address) -> bool;
    auto reset_active_control_profile() -> bool;

    FGameSettingsChanged settings_changed;
    FDisplayConfirmationChanged display_confirmation_changed;
  private:
    auto tick_display_confirmation(float delta_seconds) -> bool;
    void preview_immediate_settings(FGameSettingsState const& state);
    auto normalize_value(FGameSettingDescriptor const& descriptor,
                         FGameSettingValue const& value) const -> TOptional<FGameSettingValue>;
    auto input_user_settings() const -> USpaceGameInputUserSettings*;
    auto all_control_bindings() const -> TArray<FControlBindingView>;
    void capture_input_edit_state();
    void restore_input_edit_state();
    auto input_is_dirty() const -> bool;

    FGameSettingsBackend backend_;
    TWeakObjectPtr<ULocalPlayer> editing_local_player_{};
    FGameSettingsEditState edit_state_{};
    TArray<FControlProfileView> applied_control_profiles_{};
    TArray<FControlBindingView> applied_control_bindings_{};
    FString applied_control_profile_id_{};
    double display_confirmation_deadline_{};
    FTSTicker::FDelegateHandle display_confirmation_ticker_;
    bool editing_{};
    bool awaiting_display_confirmation_{};
};

} // namespace ml::ioj
