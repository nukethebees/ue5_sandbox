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
    float walk_speed{450.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement");
    float run_speed{600.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement");
    float acceleration{2048.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement");
    float boost_scale_factor{1.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement");
    bool is_running{false};
};
