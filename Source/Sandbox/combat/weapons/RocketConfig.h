#pragma once

#include "Sandbox/health/HealthChange.h"

#include "RocketConfig.generated.h"

USTRUCT(BlueprintType)
struct FRocketConfig {
    GENERATED_BODY()

    FRocketConfig() = default;
    FRocketConfig(float speed, float explosion_radius, FHealthChange damage)
        : speed(speed)
        , explosion_radius(explosion_radius)
        , damage(damage) {}
    FRocketConfig(float speed)
        : speed(speed) {}

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    float speed{2000.f};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    float explosion_radius{400.f};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    FHealthChange damage{50.0f, EHealthChangeType::Damage};
};
