#pragma once

#include "CoreMinimal.h"

#include "countdown_timers.generated.h"

USTRUCT()
struct SANDBOXCORE_API FCountdownTimers {
    GENERATED_BODY()

    void tick(float const dt) noexcept;

    auto operator[](int32 const i) const -> float { return remaining_times[i]; }
    auto operator[](int32 const i) -> float& { return remaining_times[i]; }

    auto Num() const noexcept { return remaining_times.Num(); }
    auto Add(float t) { remaining_times.Add(t); }
    void Add(float const t, int32 const count);
    void AddZeroed(int32 const count);
    void Reset() { remaining_times.Reset(); }
    void RemoveAtSwap(int32 const index, int32 const count, EAllowShrinking const as) {
        remaining_times.RemoveAtSwap(index, count, as);
    }

    UPROPERTY(VisibleAnywhere)
    TArray<float> remaining_times;
};
