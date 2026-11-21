#pragma once

#include <SpaceGame/ships/common/SpaceShipControllerInputs.h>

#include <InputAction.h>
#include <InputMappingContext.h>

namespace ioj::tests {
inline void
    configure_ship_inputs(FSpaceShipControllerInputs& input, UObject& owner, UInputAction& action) {
    input.starfox = NewObject<UInputMappingContext>(&owner);
    input.pitch = &action;
    input.yaw = &action;
    input.accelerate = &action;
    input.brake = &action;
    input.boost = &action;
    input.emergency_brake = &action;
    input.fire_primary = &action;
}
}
