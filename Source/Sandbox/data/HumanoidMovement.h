#pragma once

#include "CoreMinimal.h"

#include "HumanoidMovement.generated.h"

USTRUCT(BlueprintType)
struct FHumanoidMovement {
    GENERATED_BODY()

    FHumanoidMovement() = default;
    FHumanoidMovement(float walk_speed, float run_speed, float acceleration)
        : walk_speed(walk_speed)
        , run_speed(run_speed)
        , acceleration(acceleration) {}

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement");
    float walk_speed{800.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement");
    float run_speed{1200.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement");
    float acceleration{100.0f};
};
