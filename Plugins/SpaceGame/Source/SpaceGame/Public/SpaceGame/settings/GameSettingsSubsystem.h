#pragma once

#include "Containers/Ticker.h"
#include "SpaceGame/settings/GameSettings.generated.h"
#include "SpaceGame/settings/GameSettingsBackend.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "GameSettingsSubsystem.generated.h"

namespace ml::ioj {

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

    void begin_edit();
    void cancel();
    void apply();
    void reset_category(EGameSettingCategory category);
    void confirm_display_changes();
    void revert_display_changes();

    auto settings_state() const -> FGameSettingsState const&;
    auto value(EGameSetting setting) const -> FGameSettingValue;
    void set_setting(EGameSetting setting, FGameSettingValue const& value);
    auto descriptors(EGameSettingCategory category) const -> TArray<FGameSettingDescriptor const*>;
    auto options(EGameSetting setting) const -> TArray<FGameSettingOption>;
    auto is_available(EGameSetting setting) const -> bool;
    auto is_dirty() const -> bool;
    auto is_awaiting_display_confirmation() const -> bool;
    auto display_confirmation_seconds_remaining() const -> int32;

    FGameSettingsChanged settings_changed;
    FDisplayConfirmationChanged display_confirmation_changed;
  private:
    auto tick_display_confirmation(float delta_seconds) -> bool;
    void preview_immediate_settings(FGameSettingsState const& state);
    auto normalize_value(FGameSettingDescriptor const& descriptor,
                         FGameSettingValue const& value) const -> TOptional<FGameSettingValue>;

    FGameSettingsBackend backend_;
    FGameSettingsState baseline_{};
    FGameSettingsState pending_{};
    double display_confirmation_deadline_{};
    FTSTicker::FDelegateHandle display_confirmation_ticker_;
    bool editing_{};
    bool awaiting_display_confirmation_{};
};

} // namespace ml::ioj
