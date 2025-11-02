#pragma once

#include "CoreMinimal.h"

#include "PlayerUpgradeStationMode.generated.h"

UENUM(BlueprintType)
enum class EPlayerUpgradeStationMode : uint8 {
    Attributes UMETA(DisplayName = "Attributes"),
    TechSkills UMETA(DisplayName = "TechSkills"),
    WeaponSkills UMETA(DisplayName = "WeaponSkills"),
    Psi UMETA(DisplayName = "Psi"),
};
