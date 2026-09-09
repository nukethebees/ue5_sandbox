#pragma once

#include <CoreMinimal.h>

#include "HudCrosshairDistances.generated.h"

USTRUCT(BlueprintType)
struct FHudCrosshairDistances {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "UI")
    float near{3000.f};

    UPROPERTY(EditAnywhere, Category = "UI")
    float far{6000.f};
};
