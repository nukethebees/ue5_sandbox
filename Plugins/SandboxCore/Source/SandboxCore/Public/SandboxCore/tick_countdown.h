#pragma once

#include "CoreMinimal.h"

#include <concepts>
#include <limits>
#include <type_traits>

template <std::signed_integral CounterType>
class TTickCountdown {
  public:
    using size_type = int32;
    using counter_type = CounterType;

    template <typename T>
    class TView {
      public:
        using size_type = int32;
        using counter_type = std::remove_cvref_t<T>;

        static_assert(std::same_as<counter_type, CounterType>);

        TView() = default;

        [[nodiscard]] auto num() const noexcept -> size_type { return length_; }

        [[nodiscard]] auto operator[](size_type const index) const noexcept -> counter_type {
            check(index >= 0);
            check(index < length_);
            return countdown_->counters_[offset_ + index];
        }

        [[nodiscard]] auto try_consume(size_type const index) const noexcept -> bool
            requires (!std::is_const_v<T>)
        {
            check_index(index);
            return countdown_->try_consume(offset_ + index);
        }

        void set_counter(size_type const index, counter_type const value) const noexcept
            requires (!std::is_const_v<T>)
        {
            check_index(index);
            countdown_->set_counter(offset_ + index, value);
        }

        void zero_counter(size_type const index) const noexcept
            requires (!std::is_const_v<T>)
        {
            check_index(index);
            countdown_->zero_counter(offset_ + index);
        }
      private:
        using Countdown =
            std::conditional_t<std::is_const_v<T>, TTickCountdown const, TTickCountdown>;

        friend class TTickCountdown;

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

    TTickCountdown() = default;

    TTickCountdown(size_type const count, counter_type const initial_tick_value)
        : tick_value_{initial_tick_value} {
        check(initial_tick_value >= 0);
        counters_.Init(tick_value_, count);
    }

    void tick() noexcept {
        for (auto& counter : counters_) {
            --counter;
        }

        ++cleaner_counter_;
        if (cleaner_counter_ >= clean_interval_) {
            cleaner_counter_ = 0;
            for (auto& counter : counters_) {
                counter = FMath::Max(counter, counter_type{0});
            }
        }
    }

    [[nodiscard]] static auto is_ready(counter_type const value) noexcept -> bool {
        return value <= 0;
    }

    [[nodiscard]] auto try_consume(size_type const index) noexcept -> bool {
        auto& counter{counters_[index]};
        if (!is_ready(counter)) {
            return false;
        }

        counter = tick_value_;
        return true;
    }

    void consume(size_type const index) noexcept { (void)try_consume(index); }

    void consume() noexcept {
        for (auto& counter : counters_) {
            if (is_ready(counter)) {
                counter = tick_value_;
            }
        }
    }

    void reset() {
        counters_.Reset();
        cleaner_counter_ = 0;
    }

    void reserve(size_type const count) { counters_.Reserve(count); }

    void add_zeroed(size_type const count) { counters_.AddZeroed(count); }

    void add_defaulted(size_type const count) { counters_.AddDefaulted(count); }

    void add_uninitialised(size_type const count) { counters_.AddUninitialized(count); }

    void remove_at_swap(size_type const index,
                        size_type const count,
                        EAllowShrinking const allow_shrinking) {
        counters_.RemoveAtSwap(index, count, allow_shrinking);
    }

    void set_num(size_type const count, EAllowShrinking const allow_shrinking) {
        counters_.SetNum(count, allow_shrinking);
    }

    void copy_element(size_type const dst_index,
                      TTickCountdown const& src,
                      size_type const src_index) {
        counters_[dst_index] = src.counters_[src_index];
    }

    [[nodiscard]] auto num() const noexcept -> size_type { return counters_.Num(); }

    [[nodiscard]] auto tick_value() const noexcept -> counter_type { return tick_value_; }

    void set_tick_value(counter_type const value) noexcept {
        check(value >= 0);
        tick_value_ = value;
    }

    void set_counter(size_type const index, counter_type const value) noexcept {
        check(value >= 0);
        counters_[index] = value;
    }

    void zero_counter(size_type const index) noexcept { counters_[index] = 0; }

    [[nodiscard]] auto counters() noexcept -> TConstArrayView<counter_type> { return counters_; }

    [[nodiscard]] auto counters() const noexcept -> TConstArrayView<counter_type> {
        return counters_;
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
        return get_view(offset, count);
    }
  private:
    static constexpr counter_type clean_interval_{
        static_cast<counter_type>(std::numeric_limits<counter_type>::max() / 2 + 1)};

    counter_type tick_value_{0};
    counter_type cleaner_counter_{0};
    TArray<counter_type> counters_;
};

using FTickCountdown8 = TTickCountdown<int8>;
using FTickCountdown16 = TTickCountdown<int16>;
using FTickCountdown32 = TTickCountdown<int32>;
