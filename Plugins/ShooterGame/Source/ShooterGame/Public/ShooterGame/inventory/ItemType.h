#pragma once

#include "CoreMinimal.h"

#include <utility>

#include "ItemType.generated.h"

UENUM(BlueprintType)
enum class EItemType : uint8 {
    Junk UMETA(DisplayName = "Junk"),
    Weapon UMETA(DisplayName = "Weapon"),
    Ammo UMETA(DisplayName = "Junk"),
};
