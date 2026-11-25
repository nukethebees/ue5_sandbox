#include "SpaceGame/input/SpaceGameInputModifier.h"

#include "ioj/input/response.h"
#include "SpaceGame/UnrealMessages.h"

#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "GameFramework/PlayerController.h"
#include <Engine/GameInstance.h>

namespace ioj {
namespace {
auto native_response(ESpaceGameInputResponse const response) -> input::Response {
    switch (response) {
        case ESpaceGameInputResponse::TurnPointerDelta:
            return input::Response::TurnPointerDelta;
        case ESpaceGameInputResponse::GamepadTurn:
            return input::Response::GamepadTurn;
    }

    checkNoEntry();
    return input::Response::GamepadTurn;
}
auto native_axis(ESpaceGameInputAxis const axis) -> input::Axis {
    switch (axis) {
        case ESpaceGameInputAxis::None:
            return input::Axis::None;
        case ESpaceGameInputAxis::Pitch:
            return input::Axis::Pitch;
        case ESpaceGameInputAxis::Yaw:
            return input::Axis::Yaw;
    }

    checkNoEntry();
    return input::Axis::None;
}
}

auto USpaceGameInputModifier::ModifyRaw_Implementation(
    UEnhancedPlayerInput const* const player_input,
    FInputActionValue current_value,
    float const delta_time) -> FInputActionValue {
    static_cast<void>(delta_time);
    auto const& values{player_settings().applied()};
    auto const response_type{native_response(response)};
    auto const response_axis{native_axis(axis)};
    switch (current_value.GetValueType()) {
        case EInputActionValueType::Axis1D:
            return FInputActionValue{input::apply_response(
                current_value.Get<float>(), response_type, response_axis, values)};
        case EInputActionValueType::Axis2D: {
            auto const value{current_value.Get<FVector2D>()};
            auto const result{input::apply_response(
                Vector2d{value.X, value.Y}, response_type, response_axis, values)};
            return FInputActionValue{FVector2D{result.x(), result.y()}};
        }
        case EInputActionValueType::Axis3D: {
            auto const value{current_value.Get<FVector>()};
            auto const result{input::apply_response(
                Vector3d{value.X, value.Y, value.Z}, response_type, response_axis, values)};
            return FInputActionValue{FVector{result.x(), result.y(), result.z()}};
        }
        case EInputActionValueType::Boolean:
            return current_value;
    }

    return current_value;
}
}
