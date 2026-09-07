#pragma once

#include "UserSettings/EnhancedInputUserSettings.h"

#include "SpaceGameInputUserSettings.generated.h"

namespace ml::ioj {

UCLASS()
class SPACEGAME_API USpaceGameInputUserSettings final : public UEnhancedInputUserSettings {
    GENERATED_BODY()
  public:
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
  protected:
    auto DetermineHardwareDeviceForActionMapping(FEnhancedActionKeyMapping const& action_mapping,
                                                 UInputMappingContext const* mapping_context) const
        -> FHardwareDeviceIdentifier override;
  private:
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
};

} // namespace ml::ioj
