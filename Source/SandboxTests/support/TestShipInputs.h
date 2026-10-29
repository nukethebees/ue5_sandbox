#pragma once

#include <SpaceGame/ships/common/SpaceShipControllerInputs.h>

#include <InputAction.h>
#include <InputMappingContext.h>

namespace ioj::tests {
inline void
    configure_ship_inputs(FSpaceShipControllerInputs& input, UObject& owner, UInputAction& action) {
    input.starfox = NewObject<UInputMappingContext>(&owner);
    input.fighter = NewObject<UInputMappingContext>(&owner);
    input.skater = NewObject<UInputMappingContext>(&owner);
    input.gunship = NewObject<UInputMappingContext>(&owner);
    input.translate_forward = &action;
    input.translate_right = &action;
    input.translate_up = &action;
    input.pitch = &action;
    input.yaw = &action;
    input.roll = &action;
    input.accelerate = &action;
    input.brake = &action;
    input.boost = &action;
    input.emergency_brake = &action;
    input.fire_primary = &action;
    input.select_starfox = &action;
    input.select_fighter = &action;
    input.select_skater = &action;
    input.select_gunship = &action;
}
}
