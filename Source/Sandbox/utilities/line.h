#pragma once

#include "CoreMinimal.h"

#include "line.generated.h"

USTRUCT(BlueprintType)
struct FLine {
    GENERATED_BODY()

    FLine() = default;
    FLine(FVector start, FVector end)
        : start(start)
        , end(end) {}

    UPROPERTY(VisibleAnywhere)
    FVector start{FVector::ZeroVector};
    UPROPERTY(VisibleAnywhere)
    FVector end{FVector::ZeroVector};
};
