#pragma once

#include "CoreMinimal.h"

#include "Sandbox/generated/strong_typedefs/Ammo.h"

#include "AmmoReloadResult.generated.h"

USTRUCT(BlueprintType)
struct FAmmoReloadResult {
    GENERATED_BODY()

    FAmmoReloadResult() = default;
    FAmmoReloadResult(FAmmo ammo_used, FAmmo ammo_remaining)
        : ammo_used(ammo_used)
        , ammo_remaining(ammo_remaining) {}

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FAmmo ammo_used{};

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FAmmo ammo_remaining{};
};
