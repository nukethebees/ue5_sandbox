#pragma once

#include "GridLayoutShape.h"
#include "LayoutOffsetMode.h"

#include "CoreMinimal.h"

#include "LayoutSettings.generated.h"

USTRUCT()
struct FLayoutSettings {
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
    EGridLayoutShape shape{EGridLayoutShape::Cuboid};
    UPROPERTY(EditAnywhere)
    ELayoutOffsetMode offset_mode{ELayoutOffsetMode::CentreToCentre};
    UPROPERTY(EditAnywhere)
    FVector offset{};
};