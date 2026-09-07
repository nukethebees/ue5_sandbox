#pragma once

#include "UserSettings/EnhancedInputUserSettings.h"

#include "SpaceGameInputUserSettings.generated.h"

namespace ml::ioj {

UCLASS()
class SPACEGAME_API USpaceGameKeyProfile final : public UEnhancedPlayerMappableKeyProfile {
    GENERATED_BODY()
  public:
    void initialize_from(UEnhancedPlayerMappableKeyProfile const& source);
    void set_runtime_profile_id(FString const& profile_id);
};

UCLASS()
class SPACEGAME_API USpaceGameInputUserSettings final : public UEnhancedInputUserSettings {
    GENERATED_BODY()
  public:
    void Initialize(ULocalPlayer* local_player) override;
    auto SetActiveKeyProfile(FString const& profile_id) -> bool override;

    [[nodiscard]] auto mouse_turn_sensitivity() const noexcept -> float {
        return mouse_turn_sensitivity_;
    }
    void set_mouse_turn_sensitivity(float value) noexcept;

    [[nodiscard]] auto gamepad_turn_sensitivity() const noexcept -> float {
        return gamepad_turn_sensitivity_;
    }
    void set_gamepad_turn_sensitivity(float value) noexcept;

    [[nodiscard]] auto gamepad_turn_dead_zone() const noexcept -> float {
        return gamepad_turn_dead_zone_;
    }
    void set_gamepad_turn_dead_zone(float value) noexcept;

    [[nodiscard]] auto gamepad_move_dead_zone() const noexcept -> float {
        return gamepad_move_dead_zone_;
    }
    void set_gamepad_move_dead_zone(float value) noexcept;

    [[nodiscard]] auto invert_mouse_pitch() const noexcept -> bool { return invert_mouse_pitch_; }
    void set_invert_mouse_pitch(bool value) noexcept;

    [[nodiscard]] auto invert_gamepad_pitch() const noexcept -> bool {
        return invert_gamepad_pitch_;
    }
    void set_invert_gamepad_pitch(bool value) noexcept;

    auto create_custom_key_profile(FPlayerMappableKeyProfileCreationArgs const& arguments,
                                   FString const& source_profile_id)
        -> UEnhancedPlayerMappableKeyProfile*;
    auto rename_custom_key_profile(FString const& profile_id, FText const& display_name) -> bool;
    auto delete_custom_key_profile(FString const& profile_id) -> bool;
    [[nodiscard]] auto custom_key_profile_source_id(FString const& profile_id) const -> FString;
    [[nodiscard]] auto custom_key_profile_display_name(FString const& profile_id) const -> FText;
    [[nodiscard]] auto chord_key_for_mapping(FString const& profile_id,
                                             FPlayerKeyMapping const& mapping) const
        -> TOptional<FKey>;
  protected:
    auto RegisterKeyMappingsToProfile(UEnhancedPlayerMappableKeyProfile& profile,
                                      UInputMappingContext const* mapping_context) -> bool override;
    auto DetermineHardwareDeviceForActionMapping(FEnhancedActionKeyMapping const& action_mapping,
                                                 UInputMappingContext const* mapping_context) const
        -> FHardwareDeviceIdentifier override;
  private:
    void apply_active_mapping_profile_id();
    void migrate_custom_profile_metadata();
    void prune_stale_mapping_rows(UEnhancedPlayerMappableKeyProfile& profile,
                                  FString const& mapping_profile_id) const;
    void restore_active_profile_id();

    UPROPERTY(SaveGame)
    float mouse_turn_sensitivity_{0.07f};

    UPROPERTY(SaveGame)
    float gamepad_turn_sensitivity_{1.0f};

    UPROPERTY(SaveGame)
    float gamepad_turn_dead_zone_{0.25f};

    UPROPERTY(SaveGame)
    float gamepad_move_dead_zone_{0.25f};

    UPROPERTY(SaveGame)
    bool invert_mouse_pitch_{};

    UPROPERTY(SaveGame)
    bool invert_gamepad_pitch_{};

    UPROPERTY(SaveGame)
    TMap<FString, FString> custom_profile_source_ids_;

    UPROPERTY(SaveGame)
    TMap<FString, FString> custom_profile_names_;
};

} // namespace ml::ioj
