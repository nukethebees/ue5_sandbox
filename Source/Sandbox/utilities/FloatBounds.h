#pragma once

#include "CoreMinimal.h"

#include "FloatBounds.generated.h"

USTRUCT()
struct FFloatBounds {
    GENERATED_BODY()

    FFloatBounds() = default;
    FFloatBounds(float min, float max)
        : min(min)
        , max(max) {}

    auto get_rand() const -> float;

    UPROPERTY(EditAnywhere)
    float min{0.f};
    UPROPERTY(EditAnywhere)
    float max{1.f};
};
