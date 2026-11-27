#pragma once

#include "SpaceGame/settings/GameVideoSettings.h"
#include <ioj/host/unreal_messages.h>
#include <ioj/settings/game_setting.h>
#include <ioj/settings/game_setting_category.h>
#include <ioj/settings/game_setting_descriptor.h>
#include <ioj/settings/game_setting_value.h>
#include <ioj/settings/game_settings_edit_state.h>
#include <ioj/settings/game_settings_state.h>
#include <ioj/settings/settings_result.h>
#include <SpaceGame/settings/GameSettingOption.h>

#include "Delegates/DelegateCombinations.h"
#include "HAL/Platform.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include <Containers/Array.h>

#include <string_view>
#include <vector>

#include "OptionsDraft.generated.h"

class ULocalPlayer;

namespace ioj {

DECLARE_MULTICAST_DELEGATE(FGameSettingsChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(FDisplayConfirmationChanged, bool);

UCLASS()
class SPACEGAME_API UOptionsDraft final : public UObject {
    GENERATED_BODY()
  public:
    /* **************************************** */
    // Editing
    /* **************************************** */
    void load(ULocalPlayer* local_player);
    auto apply(GameSettingCategory category) -> bool;
    void preference_finished(host::PreferenceEdit const& edit, std::string_view error);
    void reset_category(GameSettingCategory category);
    void confirm_display_changes();
    void revert_display_changes();

    /* **************************************** */
    // Settings state
    /* **************************************** */
    auto value(GameSetting setting) const -> GameSettingValue;
    void set_setting(GameSetting setting, GameSettingValue const& value);
    auto descriptors(GameSettingCategory category) const -> TArray<GameSettingDescriptor const*>;
    auto options(GameSetting setting) const -> TArray<FGameSettingOption>;
    auto is_available(GameSetting setting) const -> bool;
    auto is_dirty() const -> bool;
    auto is_at_defaults(GameSettingCategory category) const -> bool;
    auto is_awaiting_display_confirmation() const -> bool;
    auto display_confirmation_seconds_remaining() const -> int32;

    FGameSettingsChanged settings_changed;
    FDisplayConfirmationChanged display_confirmation_changed;
  private:
    auto apply_settings(GameSettingCategory category, GameSettingsState const& values)
        -> SettingsResult;
    auto save(GameSettingCategory category) -> bool;

    /* **************************************** */
    // State
    /* **************************************** */
    FGameVideoSettings video_;
    TWeakObjectPtr<ULocalPlayer> editing_local_player_{};
};

} // namespace ioj
