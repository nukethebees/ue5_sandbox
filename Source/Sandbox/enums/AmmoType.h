#pragma once

#include "CoreMinimal.h"

#include "AmmoType.generated.h"

UENUM(BlueprintType)
enum class EAmmoType : uint8 {
    Bullets UMETA(DisplayName = "Bullets"),
    Shells UMETA(DisplayName = "Shells"),
    Rockets UMETA(DisplayName = "Rockets"),
    Energy UMETA(DisplayName = "Energy"),
    Grenades UMETA(DisplayName = "Grenades"),
    Plasma UMETA(DisplayName = "Plasma"),
};
