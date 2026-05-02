#pragma once

#include "CoreMinimal.h"

#include "TestUniformFieldSource.generated.h"

USTRUCT()
struct FTestUniformFieldPointSource {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Field")
    FVector coordinate{};
    UPROPERTY(EditAnywhere, Category = "Field")
    float strength{0.f};
    UPROPERTY(EditAnywhere, Category = "Field")
    float falloff{2.f}; // pow(distance, falloff)
    UPROPERTY(EditAnywhere, Category = "Field")
    FRotator rotation{FRotator::ZeroRotator};
};
