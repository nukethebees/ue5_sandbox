#pragma once

#include "CoreMinimal.h"

#include "WeaponPickupAction.generated.h"

UENUM(BlueprintType)
enum class EWeaponPickupAction : uint8 {
    Nothing UMETA(DisplayName = "Nothing"),
    EquipIfNothingEquipped UMETA(DisplayName = "EquipIfNothingEquipped"),
    Equip UMETA(DisplayName = "Equip")
};
