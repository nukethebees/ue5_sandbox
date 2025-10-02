#pragma once

#include "CoreMinimal.h"

#include "LandMineState.generated.h"

UENUM(BlueprintType)
enum class ELandMineState : uint8 {
    Active UMETA(DisplayName = "Active"),
    Warning UMETA(DisplayName = "Warning"),
    Detonating UMETA(DisplayName = "Detonating"),
    Deactivated UMETA(DisplayName = "Deactivated")
};