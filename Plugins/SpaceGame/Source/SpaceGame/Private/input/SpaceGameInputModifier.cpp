#include "SpaceGame/input/SpaceGameInputModifier.h"

#include "SpaceGame/input/SpaceGameInputUserSettings.h"

#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "GameFramework/PlayerController.h"

namespace ml::ioj {
namespace input_constants {
inline constexpr float virtual_stick_units_per_mouse_count{1.0f / 400.0f};
}

namespace {
auto input_settings(UEnhancedPlayerInput const* const player_input)
    -> USpaceGameInputUserSettings const* {
    auto const* const controller{
        player_input != nullptr ? Cast<APlayerController>(player_input->GetOuter()) : nullptr};
    auto const* const local_player{controller != nullptr ? controller->GetLocalPlayer() : nullptr};
    auto const* const subsystem{
        local_player != nullptr
            ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(local_player)
            : nullptr};
    return subsystem != nullptr ? Cast<USpaceGameInputUserSettings>(subsystem->GetUserSettings())
                                : nullptr;
}

auto apply_dead_zone(FInputActionValue const& value, float const threshold) -> FInputActionValue {
    auto const clamped_threshold{FMath::Clamp(threshold, 0.0f, 0.95f)};
    switch (value.GetValueType()) {
        case EInputActionValueType::Axis1D: {
            auto const input{value.Get<float>()};
            auto const magnitude{FMath::Abs(input)};
            auto const output{magnitude <= clamped_threshold
                                  ? 0.0f
                                  : FMath::Sign(input) * ((magnitude - clamped_threshold) /
                                                          (1.0f - clamped_threshold))};
            return FInputActionValue{output};
        }
        case EInputActionValueType::Axis2D: {
            auto const input{value.Get<FVector2D>()};
            auto const magnitude{input.Size()};
            if (magnitude <= clamped_threshold) {
                return FInputActionValue{FVector2D::ZeroVector};
            }
            auto const output_magnitude{FMath::Clamp(
                (magnitude - clamped_threshold) / (1.0f - clamped_threshold), 0.0f, 1.0f)};
            return FInputActionValue{input.GetSafeNormal() * output_magnitude};
        }
        case EInputActionValueType::Axis3D: {
            auto const input{value.Get<FVector>()};
            auto const magnitude{input.Size()};
            if (magnitude <= clamped_threshold) {
                return FInputActionValue{FVector::ZeroVector};
            }
            auto const output_magnitude{FMath::Clamp(
                (magnitude - clamped_threshold) / (1.0f - clamped_threshold), 0.0f, 1.0f)};
            return FInputActionValue{input.GetSafeNormal() * output_magnitude};
        }
        case EInputActionValueType::Boolean:
            return value;
    }
    return value;
}

auto scale_and_invert(FInputActionValue const& value, float const scale, bool const invert_y)
    -> FInputActionValue {
    switch (value.GetValueType()) {
        case EInputActionValueType::Axis1D:
            return FInputActionValue{value.Get<float>() * scale};
        case EInputActionValueType::Axis2D: {
            auto output{value.Get<FVector2D>() * scale};
            output.Y *= invert_y ? -1.0f : 1.0f;
            return FInputActionValue{output};
        }
        case EInputActionValueType::Axis3D: {
            auto output{value.Get<FVector>() * scale};
            output.Y *= invert_y ? -1.0f : 1.0f;
            return FInputActionValue{output};
        }
        case EInputActionValueType::Boolean:
            return value;
    }
    return value;
}
} // namespace

auto USpaceGameInputModifier::ModifyRaw_Implementation(
    UEnhancedPlayerInput const* const player_input,
    FInputActionValue current_value,
    float const delta_time) -> FInputActionValue {
    static_cast<void>(delta_time);
    auto const* const settings{input_settings(player_input)};
    if (settings == nullptr) {
        return current_value;
    }

    switch (response) {
        case ESpaceGameInputResponse::TurnPointerDelta:
            return scale_and_invert(current_value,
                                    input_constants::virtual_stick_units_per_mouse_count *
                                        settings->mouse_turn_sensitivity(),
                                    settings->invert_mouse_pitch());
        case ESpaceGameInputResponse::GamepadTurn:
            return scale_and_invert(
                apply_dead_zone(current_value, settings->gamepad_turn_dead_zone()),
                settings->gamepad_turn_sensitivity(),
                settings->invert_gamepad_pitch());
        case ESpaceGameInputResponse::GamepadMove:
            return apply_dead_zone(current_value, settings->gamepad_move_dead_zone());
    }
    return current_value;
}

} // namespace ml::ioj
