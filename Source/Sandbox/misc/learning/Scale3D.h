#pragma once

#include "CoreMinimal.h"

#include "Scale3D.generated.h"

USTRUCT()
struct FScale3D {
    GENERATED_BODY()

    FScale3D() noexcept = default;
    FScale3D(FVector non_uniform) noexcept;
    FScale3D(float uniform) noexcept;

    UPROPERTY(EditAnywhere, Category = "Grid")
    bool use_uniform_scale{true};
    UPROPERTY(EditAnywhere, Category = "Grid", meta = (EditCondition = "!use_uniform_scale"))
    FVector non_uniform_scale{FVector::OneVector};
    UPROPERTY(EditAnywhere, Category = "Grid", meta = (EditCondition = "use_uniform_scale"))
    float uniform_scale{1.f};

    auto get() const -> FVector;
};

