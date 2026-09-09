#pragma once
#include <CoreMinimal.h>
#include <SpaceGameSimulation/ships/common/LaserFiringState.h>
#include <SpaceGameSimulation/ships/common/SpaceShipCommon.h>

struct FPlayerReadView {
    FTransform transform;
    FTransform body_transform;
    FTransform middle_socket;
    FVector velocity;
    EBoostBrakeState boost_brake_state{};
    ELaserFiringState laser_firing_mode{};
    uint64 boost_start_sequence{};
};
