#pragma once

#include "CoreMinimal.h"

#include "TestEntityType.generated.h"

UENUM()
enum class ETestEntityType : uint8 {
    PlayerShip,
    Turret,
    CapitalShip,
    CapitalShipFighter,
    TubeSpinner,
    COUNT UMETA(Hidden),
};
