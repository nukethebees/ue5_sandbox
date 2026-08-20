#pragma once

#include "CoreMinimal.h"

#include "MobAttackMode.generated.h"

UENUM(BlueprintType)
enum class EMobAttackMode : uint8 {
    None UMETA(DisplayName = "None"),
    Melee UMETA(DisplayName = "Melee"),
    Ranged UMETA(DisplayName = "Ranged")
};
