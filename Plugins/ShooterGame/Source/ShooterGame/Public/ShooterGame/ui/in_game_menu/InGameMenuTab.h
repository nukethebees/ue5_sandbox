#pragma once

#include "CoreMinimal.h"

#include "InGameMenuTab.generated.h"

UENUM(BlueprintType)
enum class EInGameMenuTab : uint8 {
    Inventory UMETA(DisplayName = "Inventory"),
    Powers UMETA(DisplayName = "Powers"),
    Stats UMETA(DisplayName = "Stats"),
    Objectives UMETA(DisplayName = "Objectives"),
    Map UMETA(DisplayName = "Map"),
    Logs UMETA(DisplayName = "Logs"),
    Research UMETA(DisplayName = "Research"),
};
