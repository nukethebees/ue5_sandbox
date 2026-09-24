#pragma once

#include "UserSettings/EnhancedInputUserSettings.h"

#include "SpaceGameInputUserSettings.generated.h"

namespace ml::ioj {

class UControlBindingMetadata;

UCLASS()
class SPACEGAME_API USpaceGameInputUserSettings final : public UEnhancedInputUserSettings {
    GENERATED_BODY()
  public:
    void finalize_canonical_registration();

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
    [[nodiscard]] auto invert_mouse_yaw() const noexcept -> bool { return invert_mouse_yaw_; }
    void set_invert_mouse_yaw(bool value) noexcept;

    [[nodiscard]] auto invert_gamepad_pitch() const noexcept -> bool {
        return invert_gamepad_pitch_;
    }
    void set_invert_gamepad_pitch(bool value) noexcept;
    [[nodiscard]] auto invert_gamepad_yaw() const noexcept -> bool { return invert_gamepad_yaw_; }
    void set_invert_gamepad_yaw(bool value) noexcept;
    [[nodiscard]] auto invert_gamepad_roll() const noexcept -> bool { return invert_gamepad_roll_; }
    void set_invert_gamepad_roll(bool value) noexcept;
    [[nodiscard]] auto invert_gamepad_vertical_translation() const noexcept -> bool {
        return invert_gamepad_vertical_translation_;
    }
    void set_invert_gamepad_vertical_translation(bool value) noexcept;

    [[nodiscard]] auto chord_mapping_for_mapping(FString const& profile_id,
                                                 FPlayerKeyMapping const& mapping) const
        -> FPlayerKeyMapping const*;
    [[nodiscard]] auto control_binding_metadata(FString const& profile_id,
                                                FPlayerKeyMapping const& mapping) const
        -> UControlBindingMetadata const*;
  protected:
    auto DetermineHardwareDeviceForActionMapping(FEnhancedActionKeyMapping const& action_mapping,
                                                 UInputMappingContext const* mapping_context) const
        -> FHardwareDeviceIdentifier override;
  private:
    UPROPERTY(SaveGame)
    float mouse_turn_sensitivity_{0.25f};

    UPROPERTY(SaveGame)
    float gamepad_turn_sensitivity_{1.0f};

    UPROPERTY(SaveGame)
    float gamepad_turn_dead_zone_{0.25f};

    UPROPERTY(SaveGame)
    float gamepad_move_dead_zone_{0.25f};

    UPROPERTY(SaveGame)
    bool invert_mouse_pitch_{};
    UPROPERTY(SaveGame)
    bool invert_mouse_yaw_{};

    UPROPERTY(SaveGame)
    bool invert_gamepad_pitch_{};
    UPROPERTY(SaveGame)
    bool invert_gamepad_yaw_{};
    UPROPERTY(SaveGame)
    bool invert_gamepad_roll_{};
    UPROPERTY(SaveGame)
    bool invert_gamepad_vertical_translation_{};
};

} // namespace ml::ioj
