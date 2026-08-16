#pragma once

#include <SandboxCore/array_utils.h>
#include <SandboxCore/soa_permutation.h>

#include <Containers/ArrayView.h>
#include "CoreMinimal.h"

#include <concepts>
#include <type_traits>
#include <utility>

template <std::signed_integral CounterType>
class TPeriodicTickCountdown {
  public:
    using size_type = int32;
    using counter_type = CounterType;

    template <typename T>
    class TView {
      public:
        using View = TView<T>;
        using ConstView = TView<counter_type const>;

        TView() = default;

        [[nodiscard]] auto num() const noexcept -> size_type { return length_; }

        [[nodiscard]] auto get_view() noexcept -> View { return {*countdown_, offset_, length_}; }

        [[nodiscard]] auto get_view(size_type const offset, size_type const count) noexcept
            -> View {
            check(offset >= 0);
            check(count >= 0);
            check(offset + count <= length_);
            return {*countdown_, offset_ + offset, count};
        }

        [[nodiscard]] auto get_view() const noexcept -> ConstView { return get_const_view(); }

        [[nodiscard]] auto get_view(size_type const offset, size_type const count) const noexcept
            -> ConstView {
            return get_const_view(offset, count);
        }

        [[nodiscard]] auto get_const_view() const noexcept -> ConstView {
            return {*countdown_, offset_, length_};
        }

        [[nodiscard]] auto get_const_view(size_type const offset,
                                          size_type const count) const noexcept -> ConstView {
            check(offset >= 0);
            check(count >= 0);
            check(offset + count <= length_);
            return {*countdown_, offset_ + offset, count};
        }

        [[nodiscard]] auto operator[](size_type const index) const noexcept -> counter_type {
            check_index(index);
            return countdown_->remaining_ticks[offset_ + index];
        }

        [[nodiscard]] auto is_ready(size_type const index) const noexcept -> bool {
            return (*this)[index] <= 0;
        }

        [[nodiscard]] auto try_consume(size_type const index) const noexcept -> bool
            requires (!std::is_const_v<T>)
        {
            check_index(index);
            auto const actual_index{offset_ + index};
            if (countdown_->remaining_ticks[actual_index] > 0) {
                return false;
            }

            countdown_->remaining_ticks[actual_index] = countdown_->periods[actual_index];
            return true;
        }
      private:
        using Countdown = std::
            conditional_t<std::is_const_v<T>, TPeriodicTickCountdown const, TPeriodicTickCountdown>;

        friend class TPeriodicTickCountdown;
        template <typename>
        friend class TView;

        TView(Countdown& countdown, size_type const offset, size_type const length) noexcept
            : countdown_{&countdown}
            , offset_{offset}
            , length_{length} {
            check(offset_ >= 0);
            check(length_ >= 0);
            check(offset_ + length_ <= countdown_->num());
        }

        void check_index(size_type const index) const noexcept {
            check(index >= 0);
            check(index < length_);
        }

        Countdown* countdown_{nullptr};
        size_type offset_{0};
        size_type length_{0};
    };

    using View = TView<counter_type>;
    using ConstView = TView<counter_type const>;

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

    void initialise_last(counter_type const period, size_type const count) {
        check(count >= 0);
        check(count <= num());
        check(period > 0);

        auto const first_index{num() - count};
        for (size_type i{first_index}; i < num(); ++i) {
            remaining_ticks[i] = 0;
            periods[i] = period;
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

    void add_uninitialised(size_type const count) {
        remaining_ticks.AddUninitialized(count);
        periods.AddUninitialized(count);
    }

    void add_defaulted(size_type const count) {
        remaining_ticks.AddDefaulted(count);
        periods.AddDefaulted(count);
    }

    void set_num(size_type const count, EAllowShrinking const allow_shrinking) {
        remaining_ticks.SetNum(count, allow_shrinking);
        periods.SetNum(count, allow_shrinking);
    }

    void copy_element(size_type const dst_index,
                      TPeriodicTickCountdown const& src,
                      size_type const src_index) {
        remaining_ticks[dst_index] = src.remaining_ticks[src_index];
        periods[dst_index] = src.periods[src_index];
    }

    void copy_elements(size_type const dst_index,
                       TPeriodicTickCountdown const& src,
                       size_type const src_index,
                       size_type const count) {
        for (size_type i{}; i < count; ++i) {
            copy_element(dst_index + i, src, src_index + i);
        }
    }

    void apply_permutation(TArrayView<int32> indices) {
        ml::apply_permutation(remaining_ticks, indices);
        ml::apply_permutation(periods, indices);
    }

    [[nodiscard]] auto get_view() noexcept -> View { return {*this, 0, num()}; }

    [[nodiscard]] auto get_view() const noexcept -> ConstView { return {*this, 0, num()}; }

    [[nodiscard]] auto get_view(size_type const offset, size_type const count) noexcept -> View {
        return {*this, offset, count};
    }

    [[nodiscard]] auto get_view(size_type const offset, size_type const count) const noexcept
        -> ConstView {
        return {*this, offset, count};
    }

    [[nodiscard]] auto get_const_view(size_type const offset, size_type const count) const noexcept
        -> ConstView {
        return {*this, offset, count};
    }

    TArray<counter_type> remaining_ticks;
    TArray<counter_type> periods;
};

using FPeriodicTickCountdown8 = TPeriodicTickCountdown<int8>;
using FPeriodicTickCountdown16 = TPeriodicTickCountdown<int16>;
using FPeriodicTickCountdown32 = TPeriodicTickCountdown<int32>;
