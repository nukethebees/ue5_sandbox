#pragma once

#include "Sandbox/core/Cooldown.h"

#include <CoreMinimal.h>

#include "BurstFire.generated.h"

UENUM()
enum class EBurstFireState {
    ready,
    shot_cooldown,
    burst_cooldown,
};

USTRUCT()
struct FBurstFire {
    GENERATED_BODY()

    FBurstFire() = default;

    void tick(float dt);
    auto can_fire() const -> bool;
    auto fire() -> bool;

    UPROPERTY(EditAnywhere)
    FCooldown shot_cooldown{0.33f};
    UPROPERTY(EditAnywhere)
    FCooldown burst_cooldown{0.33f};
    UPROPERTY(EditAnywhere)
    int32 burst_size{3};
    UPROPERTY(VisibleAnywhere)
    int32 shots_remaining{3};
    UPROPERTY(VisibleAnywhere)
    EBurstFireState state{EBurstFireState::ready};
    
};
