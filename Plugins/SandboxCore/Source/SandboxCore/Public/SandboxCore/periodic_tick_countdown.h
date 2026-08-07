#pragma once

#include "CoreMinimal.h"

#include <concepts>

template <std::signed_integral CounterType>
class TPeriodicTickCountdown {
  public:
    using size_type = int32;
    using counter_type = CounterType;

    TPeriodicTickCountdown() = default;

    [[nodiscard]] auto num() const noexcept -> size_type { return remaining_ticks.Num(); }

    [[nodiscard]] auto operator[](size_type const index) const noexcept -> counter_type {
        return remaining_ticks[index];
    }

    void tick() noexcept {
        for (auto& remaining_tick : remaining_ticks) {
            --remaining_tick;
            remaining_tick = FMath::Max(remaining_tick, counter_type{0});
        }
    }

    [[nodiscard]] auto is_ready(size_type const index) const noexcept -> bool {
        return remaining_ticks[index] <= 0;
    }

    [[nodiscard]] auto try_consume(size_type const index) noexcept -> bool {
        if (!is_ready(index)) {
            return false;
        }

        remaining_ticks[index] = periods[index];
        return true;
    }

    void reset() {
        remaining_ticks.Reset();
        periods.Reset();
    }

    void add_started(counter_type const period) {
        check(period > 0);
        remaining_ticks.Add(period);
        periods.Add(period);
    }

    void add_zeroed(counter_type const period) {
        check(period > 0);
        remaining_ticks.Add(0);
        periods.Add(period);
    }

    void add_started(counter_type const period, size_type const count) {
        check(count >= 0);
        for (size_type i{0}; i < count; ++i) {
            add_started(period);
        }
    }

    void add_zeroed(counter_type const period, size_type const count) {
        check(count >= 0);
        for (size_type i{0}; i < count; ++i) {
            add_zeroed(period);
        }
    }

    [[nodiscard]] auto counters() const noexcept -> TConstArrayView<counter_type> {
        return remaining_ticks;
    }

    void remove_at_swap(size_type const index,
                        size_type const count,
                        EAllowShrinking const allow_shrinking) {
        remaining_ticks.RemoveAtSwap(index, count, allow_shrinking);
        periods.RemoveAtSwap(index, count, allow_shrinking);
    }

    void reserve(size_type const count) {
        remaining_ticks.Reserve(count);
        periods.Reserve(count);
    }

    TArray<counter_type> remaining_ticks;
    TArray<counter_type> periods;
};

using FPeriodicTickCountdown8 = TPeriodicTickCountdown<int8>;
using FPeriodicTickCountdown16 = TPeriodicTickCountdown<int16>;
using FPeriodicTickCountdown32 = TPeriodicTickCountdown<int32>;
