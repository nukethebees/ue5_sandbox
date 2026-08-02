#pragma once

#include "TestSpaceShipControlMode.generated.h"

UENUM()
enum class ETestSpaceShipControlMode : uint8 {
    Velocity,
    Power,
    COUNT UMETA(Hidden),
};
