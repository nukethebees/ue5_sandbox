#pragma once

#include "sandbox/core/countdown.h"
#include "sandbox/core/soa_permutation.h"

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace ml {
template <std::signed_integral Counter>
class TickCountdownView {
  public:
    using counter_type = std::remove_const_t<Counter>;

    TickCountdownView() = default;
    TickCountdownView(std::span<Counter> const counters, counter_type const restart_value) noexcept
        : counters_{counters}
        , restart_value_{restart_value} {
        assert(restart_value_ >= 0);
    }

    [[nodiscard]] auto num() const noexcept -> std::size_t { return counters_.size(); }

    [[nodiscard]] auto operator[](std::size_t const index) const noexcept -> counter_type {
        assert(index < num());
        return counters_[index];
    }

    [[nodiscard]] auto is_ready(std::size_t const index) const noexcept -> bool {
        return (*this)[index] <= 0;
    }

    [[nodiscard]] auto try_consume(std::size_t const index) const noexcept -> bool
        requires (!std::is_const_v<Counter>)
    {
        if (!is_ready(index)) {
            return false;
        }

        restart_counter(index);
        return true;
    }

    void restart_counter(std::size_t const index) const noexcept
        requires (!std::is_const_v<Counter>)
    {
        set_counter(index, restart_value_);
    }

    void set_counter(std::size_t const index, counter_type const value) const noexcept
        requires (!std::is_const_v<Counter>)
    {
        assert(index < num());
        assert(value >= 0);
        counters_[index] = value;
    }

    void zero_counter(std::size_t const index) const noexcept
        requires (!std::is_const_v<Counter>)
    {
        set_counter(index, 0);
    }
  private:
    std::span<Counter> counters_;
    counter_type restart_value_{};
};

template <std::signed_integral Counter>
class TickCountdown {
  public:
    using size_type = std::int32_t;
    using counter_type = Counter;
    using View = TickCountdownView<counter_type>;
    using ConstView = TickCountdownView<counter_type const>;

    TickCountdown() = default;

    TickCountdown(size_type const count, counter_type const initial_tick_value)
        : tick_value_{initial_tick_value} {
        assert(count >= 0);
        assert(initial_tick_value >= 0);
        counters_.resize(static_cast<std::size_t>(count), tick_value_);
    }

    void tick() noexcept {
        ml::tick_countdowns<counter_type>(counters(), cleaner_counter_, clean_interval_);
    }

    [[nodiscard]] auto is_ready(size_type const index) const noexcept -> bool {
        return get_view().is_ready(to_index(index));
    }

    template <std::integral TickType>
    [[nodiscard]] static constexpr auto tick_can_fit(TickType const value) noexcept -> bool {
        if constexpr (std::signed_integral<TickType>) {
            if (value < 0) {
                return false;
            }
        }

        return std::in_range<counter_type>(value);
    }

    [[nodiscard]] auto try_consume(size_type const index) noexcept -> bool {
        return get_view().try_consume(to_index(index));
    }

    void consume(size_type const index) noexcept { (void)try_consume(index); }

    void consume() noexcept { ml::consume_ready_countdowns(counters(), tick_value_); }

    void reset() {
        counters_.clear();
        cleaner_counter_ = 0;
    }

    void reserve(size_type const count) {
        assert(count >= 0);
        counters_.reserve(static_cast<std::size_t>(count));
    }

    void add_zeroed(size_type const count) {
        assert(count >= 0);
        counters_.resize(counters_.size() + static_cast<std::size_t>(count));
    }

    void add_defaulted(size_type const count) { add_zeroed(count); }

    void add_uninitialised(size_type const count) { add_zeroed(count); }

    void remove_at_swap(size_type const index, size_type const count) {
        assert(index >= 0);
        assert(count >= 0);
        assert(static_cast<std::size_t>(index) <= counters_.size());
        assert(static_cast<std::size_t>(count) <=
               counters_.size() - static_cast<std::size_t>(index));

        auto const old_size{counters_.size()};
        auto const tail{old_size - static_cast<std::size_t>(index) -
                        static_cast<std::size_t>(count)};
        auto const moved{std::min(tail, static_cast<std::size_t>(count))};
        auto const source{old_size - moved};
        for (std::size_t offset{}; offset < moved; ++offset) {
            counters_[static_cast<std::size_t>(index) + offset] = counters_[source + offset];
        }
        counters_.resize(old_size - static_cast<std::size_t>(count));
    }

    void set_num(size_type const count) {
        assert(count >= 0);
        counters_.resize(static_cast<std::size_t>(count));
    }

    void copy_element(size_type const destination,
                      TickCountdown const& source,
                      size_type const source_index) {
        counters_[to_index(destination)] = source.counters_[source.to_index(source_index)];
    }

    void apply_permutation(std::span<std::int32_t> const indices) {
        ml::apply_permutation(counters(), indices);
    }

    [[nodiscard]] auto num() const noexcept -> size_type {
        return static_cast<size_type>(counters_.size());
    }

    [[nodiscard]] auto tick_value() const noexcept -> counter_type { return tick_value_; }

    template <std::integral TickType>
    void set_tick_value(TickType const value) noexcept {
        assert(tick_can_fit(value));
        tick_value_ = static_cast<counter_type>(value);
    }

    void restart_counter(size_type const index) noexcept {
        get_view().restart_counter(to_index(index));
    }

    void set_counter(size_type const index, counter_type const value) noexcept {
        get_view().set_counter(to_index(index), value);
    }

    void zero_counter(size_type const index) noexcept { get_view().zero_counter(to_index(index)); }

    void zero_last(size_type const count) noexcept {
        assert(count >= 0);
        assert(static_cast<std::size_t>(count) <= counters_.size());

        auto const first{counters_.size() - static_cast<std::size_t>(count)};
        for (auto& counter : std::span{counters_}.subspan(first)) {
            counter = 0;
        }
    }

    [[nodiscard]] auto counters() noexcept -> std::span<counter_type> {
        return {counters_.data(), counters_.size()};
    }

    [[nodiscard]] auto counters() const noexcept -> std::span<counter_type const> {
        return {counters_.data(), counters_.size()};
    }

    [[nodiscard]] auto get_view() noexcept -> View { return {counters(), tick_value_}; }

    [[nodiscard]] auto get_view() const noexcept -> ConstView { return {counters(), tick_value_}; }
  private:
    [[nodiscard]] auto to_index(size_type const index) const noexcept -> std::size_t {
        assert(index >= 0);
        assert(static_cast<std::size_t>(index) < counters_.size());
        return static_cast<std::size_t>(index);
    }

    static constexpr counter_type clean_interval_{
        static_cast<counter_type>(std::numeric_limits<counter_type>::max() / 2 + 1)};

    counter_type tick_value_{0};
    counter_type cleaner_counter_{0};
    std::vector<counter_type> counters_;
};
}
