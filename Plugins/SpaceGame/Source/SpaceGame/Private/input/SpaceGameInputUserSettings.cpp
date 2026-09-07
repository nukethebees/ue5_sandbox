#include "SpaceGame/input/SpaceGameInputUserSettings.h"

#include "EnhancedActionKeyMapping.h"

namespace ml::ioj {

void USpaceGameInputUserSettings::set_mouse_turn_sensitivity(float const value) noexcept {
    mouse_turn_sensitivity_ = FMath::Max(0.0f, value);
}

void USpaceGameInputUserSettings::set_gamepad_turn_sensitivity(float const value) noexcept {
    gamepad_turn_sensitivity_ = FMath::Max(0.0f, value);
}

void USpaceGameInputUserSettings::set_gamepad_turn_dead_zone(float const value) noexcept {
    gamepad_turn_dead_zone_ = FMath::Clamp(value, 0.0f, 0.95f);
}

void USpaceGameInputUserSettings::set_gamepad_move_dead_zone(float const value) noexcept {
    gamepad_move_dead_zone_ = FMath::Clamp(value, 0.0f, 0.95f);
}

void USpaceGameInputUserSettings::set_invert_mouse_pitch(bool const value) noexcept {
    invert_mouse_pitch_ = value;
}

void USpaceGameInputUserSettings::set_invert_gamepad_pitch(bool const value) noexcept {
    invert_gamepad_pitch_ = value;
}

auto USpaceGameInputUserSettings::DetermineHardwareDeviceForActionMapping(
    FEnhancedActionKeyMapping const& action_mapping,
    UInputMappingContext const* const mapping_context) const -> FHardwareDeviceIdentifier {
    static_cast<void>(mapping_context);
    return action_mapping.Key.IsGamepadKey() ? FHardwareDeviceIdentifier::DefaultGamepad
                                             : FHardwareDeviceIdentifier::DefaultKeyboardAndMouse;
}

} // namespace ml::ioj
