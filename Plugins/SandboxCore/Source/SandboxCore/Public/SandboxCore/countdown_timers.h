#pragma once

#include "CoreMinimal.h"

#include "countdown_timers.generated.h"

USTRUCT()
struct SANDBOXCORE_API FCountdownTimers {
    GENERATED_BODY()

    void tick(float const dt) noexcept;

    auto operator[](int32 const i) const -> float { return remaining_times[i]; }

    auto Num() const noexcept { return remaining_times.Num(); }
    auto Add(float t) { remaining_times.Add(t); }

    UPROPERTY(VisibleAnywhere)
    TArray<float> remaining_times;
};
