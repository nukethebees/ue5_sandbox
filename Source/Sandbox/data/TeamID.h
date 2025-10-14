#pragma once

#include "CoreMinimal.h"

#include "TeamID.generated.h"

UENUM(BlueprintType)
enum class ETeamID : uint8 {
    Player = 0 UMETA(DisplayName = "Player"),
    Friendly = 1 UMETA(DisplayName = "Friendly"),
    Enemy = 2 UMETA(DisplayName = "Enemy"),
    Neutral = 3 UMETA(DisplayName = "Neutral")
};
