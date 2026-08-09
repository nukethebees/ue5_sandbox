#pragma once

#include <SandboxCore/array_utils.h>
#include "CoreMinimal.h"

#include <concepts>
#include <utility>

template <std::signed_integral CounterType>
class TPeriodicTickCountdown {
  public:
    using size_type = int32;
    using counter_type = CounterType;

    TPeriodicTickCountdown() = default;

    template <std::integral PeriodType>
    [[nodiscard]] static constexpr auto valid_period(PeriodType const period) noexcept -> bool {
        return std::in_range<counter_type>(period) && period > 0;
    }

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

    void tick(counter_type const num_ticks) noexcept {
        check(num_ticks >= counter_type{0});
        if (num_ticks <= counter_type{0}) {
            return;
        }

        for (auto& remaining_tick : remaining_ticks) {
            if (remaining_tick <= num_ticks) {
                remaining_tick = counter_type{0};
                continue;
            }

            remaining_tick -= num_ticks;
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

    template <std::integral PeriodType>
    void add_started(PeriodType const period) {
        check(valid_period(period));
        auto const checked_period{static_cast<counter_type>(period)};
        remaining_ticks.Add(checked_period);
        periods.Add(checked_period);
    }

    void add_zeroed(counter_type const period) {
        check(period > 0);
        remaining_ticks.Add(0);
        periods.Add(period);
    }

    template <std::integral PeriodType>
    void add_started(PeriodType const period, size_type const count) {
        check(count >= 0);
        check(valid_period(period));
        auto const checked_period{static_cast<counter_type>(period)};
        auto const n{count};
        remaining_ticks.AddUninitialized(n);
        periods.AddUninitialized(n);
        ml::fill_last(remaining_ticks, checked_period, n);
        ml::fill_last(periods, checked_period, n);
    }

    void add_zeroed(counter_type const period, size_type const count) {
        check(count >= 0);
        check(period > 0);
        auto const n{count};
        remaining_ticks.AddZeroed(n);
        periods.AddUninitialized(n);
        ml::fill_last(periods, period, n);
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
