#pragma once

#include "CoreMinimal.h"

#include "Cooldown.generated.h"

USTRUCT(BlueprintType)
struct FCooldown {
    GENERATED_BODY()

    FCooldown() = default;
    FCooldown(float duration)
        : duration(duration) {}

    auto is_finished() const { return remaining <= 0.f; }
    void reset() { remaining = duration; }
    void finish() { remaining = -1.f; }
    void tick(float delta_time) { remaining -= delta_time; }

    auto& operator-=(float dt) {
        tick(dt);
        return *this;
    }

    UPROPERTY(EditAnywhere)
    float duration{1.f};
    UPROPERTY(VisibleAnywhere)
    float remaining{0.f};
};
