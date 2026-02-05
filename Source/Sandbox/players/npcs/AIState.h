#pragma once

#include "CoreMinimal.h"

#include "AIState.generated.h"

UENUM(BlueprintType)
enum class EAIState : uint8 {
    Idle UMETA(DisplayName = "Idle"),
    RandomlyMove UMETA(DisplayName = "Randomly Move"),
    Patrol UMETA(DisplayName = "Patrol"),
    DefendActor UMETA(DisplayName = "Defend Actor"),
    DefendPosition UMETA(DisplayName = "Defend Position"),
    Alert UMETA(DisplayName = "Alert"),
    Combat UMETA(DisplayName = "Combat"),
    Investigate UMETA(DisplayName = "Investigate"),
    SearchForLostEnemy UMETA(DisplayName = "Search For Lost Enemy"),
    Flee UMETA(DisplayName = "Flee"),
};
