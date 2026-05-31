#pragma once

#include "CoreMinimal.h"

#include "countdown_timers.generated.h"

USTRUCT()
struct FCountdownTimers {
    GENERATED_BODY()

    void tick(float const dt) noexcept;

    UPROPERTY(VisibleAnywhere)
    TArray<float> remaining_times;
};
