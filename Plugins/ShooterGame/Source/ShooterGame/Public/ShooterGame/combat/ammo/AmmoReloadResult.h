#pragma once

#include "CoreMinimal.h"

#include "ShooterGame/combat/ammo/AmmoData.h"

#include "AmmoReloadResult.generated.h"

USTRUCT(BlueprintType)
struct FAmmoReloadResult {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FAmmoData AmmoTaken{};

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FAmmoData AmmoOfferedRemaining{};
};
