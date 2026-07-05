#pragma once

#include <CoreMinimal.h>

#include "BoxSize.generated.h"

USTRUCT()
struct FBoxSize {
    GENERATED_BODY()

    FBoxSize() = default;

    UPROPERTY(EditAnywhere)
    FVector box_size{FVector::ZeroVector};
};
