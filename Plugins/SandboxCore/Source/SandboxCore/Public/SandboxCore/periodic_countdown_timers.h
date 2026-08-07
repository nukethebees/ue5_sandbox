#pragma once

#include "CoreMinimal.h"

struct SANDBOXCORE_API FPeriodicCountdownTimers {
    using View = TArrayView<float>;
    using ConstView = TConstArrayView<float>;

    void tick(float const dt) noexcept;

    auto expired(int32 const index) const noexcept -> bool { return remaining_times[index] <= 0.f; }
    void reset(int32 const index) noexcept { remaining_times[index] = periods[index]; }
    auto try_consume(int32 const index) noexcept -> bool {
        if (!expired(index)) {
            return false;
        }

        reset(index);
        return true;
    }

    auto operator[](int32 const index) const -> float { return remaining_times[index]; }
    auto operator[](int32 const index) -> float& { return remaining_times[index]; }

    auto get_view() -> View { return remaining_times; }
    auto get_view(int32 const offset, int32 const count) -> View {
        return get_view().Slice(offset, count);
    }
    auto get_view() const -> ConstView { return remaining_times; }
    auto get_view(int32 const offset, int32 const count) const -> ConstView {
        return get_view().Slice(offset, count);
    }
    auto get_const_view() const -> ConstView { return remaining_times; }
    auto get_const_view(int32 const offset, int32 const count) const -> ConstView {
        return get_const_view().Slice(offset, count);
    }

    auto Num() const noexcept { return remaining_times.Num(); }
    void add_started(float const period);
    void add_started(float const period, int32 const count);
    void add_started(ConstView const new_periods);
    void add_zeroed(float const period);
    void add_zeroed(float const period, int32 const count);
    void add_zeroed(ConstView const new_periods);
    void Reset() {
        remaining_times.Reset();
        periods.Reset();
    }
    void RemoveAtSwap(int32 const index, int32 const count, EAllowShrinking const as) {
        remaining_times.RemoveAtSwap(index, count, as);
        periods.RemoveAtSwap(index, count, as);
    }
    auto AddUninitialized(int32 const count) -> int32 {
        auto const offset{remaining_times.AddUninitialized(count)};
        periods.AddUninitialized(count);
        return offset;
    }
    void copy_element(int32 const dst_i, FPeriodicCountdownTimers const& src, int32 const src_i) {
        remaining_times[dst_i] = src.remaining_times[src_i];
        periods[dst_i] = src.periods[src_i];
    }

    void Append(ConstView const new_remaining_times, ConstView const new_periods);

    TArray<float> remaining_times;
    TArray<float> periods;
};
