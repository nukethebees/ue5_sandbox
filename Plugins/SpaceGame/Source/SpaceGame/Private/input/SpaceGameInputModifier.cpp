#include "SpaceGame/input/SpaceGameInputModifier.h"

#include "ioj/input/response.h"
#include "SpaceGame/input/SpaceGameInputUserSettings.h"

#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "GameFramework/PlayerController.h"

namespace ioj {
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

auto native_response(ESpaceGameInputResponse const response) -> input::Response {
    switch (response) {
        case ESpaceGameInputResponse::TurnPointerDelta:
            return input::Response::TurnPointerDelta;
        case ESpaceGameInputResponse::GamepadTurn:
            return input::Response::GamepadTurn;
        case ESpaceGameInputResponse::GamepadMove:
            return input::Response::GamepadMove;
    }
    checkNoEntry();
    return input::Response::GamepadMove;
}
auto native_axis(ESpaceGameInputAxis const axis) -> input::Axis {
    switch (axis) {
        case ESpaceGameInputAxis::None:
            return input::Axis::None;
        case ESpaceGameInputAxis::Pitch:
            return input::Axis::Pitch;
        case ESpaceGameInputAxis::Yaw:
            return input::Axis::Yaw;
        case ESpaceGameInputAxis::Roll:
            return input::Axis::Roll;
        case ESpaceGameInputAxis::VerticalTranslation:
            return input::Axis::VerticalTranslation;
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
    auto const* const settings{input_settings(player_input)};
    if (settings == nullptr) {
        return current_value;
    }
    auto const& values{settings->response_settings()};
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
