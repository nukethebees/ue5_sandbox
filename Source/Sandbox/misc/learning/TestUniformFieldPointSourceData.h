#pragma once

#include "CoreMinimal.h"

#include <limits>

#include "TestUniformFieldPointSourceData.generated.h"

USTRUCT()
struct FTestUniformFieldPointSourceData {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Field")
    FVector coordinate{};
    UPROPERTY(EditAnywhere, Category = "Field")
    float strength{0.f}; // Positive is repulsive
    UPROPERTY(EditAnywhere, Category = "Field")
    float falloff{-2.f}; // pow(distance, falloff)
    UPROPERTY(EditAnywhere, Category = "Field")
    FRotator rotation{FRotator::ZeroRotator};
    // The field is only present within the two radii
    UPROPERTY(EditAnywhere, Category = "Field")
    float inner_radius{0};
    UPROPERTY(EditAnywhere, Category = "Field")
    float outer_radius{std::numeric_limits<float>::max()};
};
