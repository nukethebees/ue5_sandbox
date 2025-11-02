#pragma once

#include "CoreMinimal.h"

#include "PlayerWeaponSkills.generated.h"

USTRUCT(BlueprintType)
struct FPlayerWeaponSkills {
    GENERATED_BODY()

    FPlayerWeaponSkills() = default;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player")
    int32 small_guns{1};
};
