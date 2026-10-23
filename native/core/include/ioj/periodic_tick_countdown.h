#pragma once

#include "ioj/constrained_numbers.h"
#include "ioj/container_ops.h"
#include "ioj/countdown.h"
#include "ioj/soa_permutation.h"

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace ioj {
template <std::signed_integral Counter>
class PeriodicTickCountdownView {
  public:
    using counter_type = std::remove_const_t<Counter>;

    PeriodicTickCountdownView() = default;
    PeriodicTickCountdownView(std::span<Counter> const remaining_ticks,
                              std::span<counter_type const> const periods) noexcept
        : remaining_ticks_{remaining_ticks}
        , periods_{periods} {
        assert(remaining_ticks_.size() == periods_.size());
    }

    [[nodiscard]] auto num() const noexcept -> std::size_t { return remaining_ticks_.size(); }

    [[nodiscard]] auto operator[](std::size_t const index) const noexcept -> counter_type {
        assert(index < num());
        return remaining_ticks_[index];
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

        assert(index < periods_.size());
        remaining_ticks_[index] = periods_[index];
        return true;
    }
  private:
    std::span<Counter> remaining_ticks_;
    std::span<counter_type const> periods_;
};

template <std::signed_integral Counter>
class PeriodicTickCountdown {
  public:
    using size_type = std::uint32_t;
    using counter_type = Counter;
    using period_type = Positive<counter_type>;
    using View = PeriodicTickCountdownView<counter_type>;
    using ConstView = PeriodicTickCountdownView<counter_type const>;

    PeriodicTickCountdown() = default;

    [[nodiscard]] auto num() const noexcept -> size_type {
        return static_cast<size_type>(remaining_ticks_.size());
    }

    [[nodiscard]] auto operator[](size_type const index) const noexcept -> counter_type {
        return remaining_ticks_[to_index(index)];
    }

    void tick() noexcept { tick_periodic_countdowns(remaining_ticks()); }

    void tick(counter_type const num_ticks) noexcept {
        assert(num_ticks >= 0);
        tick_periodic_countdowns(remaining_ticks(), num_ticks);
    }

    [[nodiscard]] auto is_ready(size_type const index) const noexcept -> bool {
        return remaining_ticks_[to_index(index)] <= 0;
    }

    [[nodiscard]] auto try_consume(size_type const index) noexcept -> bool {
        return get_view().try_consume(to_index(index));
    }

    void reset() {
        remaining_ticks_.clear();
        periods_.clear();
    }

    void add_started(period_type const period) {
        remaining_ticks_.push_back(period);
        periods_.push_back(period);
    }

    void add_zeroed(period_type const period) {
        remaining_ticks_.push_back(0);
        periods_.push_back(period);
    }

    void add_started(period_type const period, size_type const count) {
        auto const n{static_cast<std::size_t>(count)};
        remaining_ticks_.insert(remaining_ticks_.end(), n, period);
        periods_.insert(periods_.end(), n, period);
    }

    void add_zeroed(period_type const period, size_type const count) {
        auto const n{static_cast<std::size_t>(count)};
        remaining_ticks_.insert(remaining_ticks_.end(), n, 0);
        periods_.insert(periods_.end(), n, period);
    }

    void initialise_last(period_type const period, size_type const count) {
        assert(static_cast<std::size_t>(count) <= remaining_ticks_.size());

        auto const first{remaining_ticks_.size() - static_cast<std::size_t>(count)};
        for (std::size_t index{first}; index < remaining_ticks_.size(); ++index) {
            remaining_ticks_[index] = 0;
            periods_[index] = period;
        }
    }

    void remove_at_swap(size_type const index, size_type const count) {
        ioj::remove_at_swap(remaining_ticks_, index, count);
        ioj::remove_at_swap(periods_, index, count);
    }

    void reserve(size_type const count) {
        auto const capacity{static_cast<std::size_t>(count)};
        remaining_ticks_.reserve(capacity);
        periods_.reserve(capacity);
    }

    void add_uninitialised(size_type const count) {
        auto const n{static_cast<std::size_t>(count)};
        remaining_ticks_.resize(remaining_ticks_.size() + n);
        periods_.resize(periods_.size() + n);
    }

    void add_defaulted(size_type const count) { add_uninitialised(count); }

    void set_num(size_type const count) {
        auto const n{static_cast<std::size_t>(count)};
        remaining_ticks_.resize(n);
        periods_.resize(n);
    }

    void copy_element(size_type const destination,
                      PeriodicTickCountdown const& source,
                      size_type const source_index) {
        auto const destination_index{to_index(destination)};
        auto const source_offset{source.to_index(source_index)};
        remaining_ticks_[destination_index] = source.remaining_ticks_[source_offset];
        periods_[destination_index] = source.periods_[source_offset];
    }

    void copy_elements(size_type const destination,
                       PeriodicTickCountdown const& source,
                       size_type const source_index,
                       size_type const count) {
        for (size_type offset{}; offset < count; ++offset) {
            copy_element(destination + offset, source, source_index + offset);
        }
    }

    void apply_permutation(std::span<std::int32_t> const indices) {
        ioj::apply_permutation(remaining_ticks(), indices);
        ioj::apply_permutation(periods(), indices);
    }

    [[nodiscard]] auto remaining_ticks() noexcept -> std::span<counter_type> {
        return {remaining_ticks_.data(), remaining_ticks_.size()};
    }

    [[nodiscard]] auto remaining_ticks() const noexcept -> std::span<counter_type const> {
        return {remaining_ticks_.data(), remaining_ticks_.size()};
    }

    [[nodiscard]] auto periods() noexcept -> std::span<counter_type> {
        return {periods_.data(), periods_.size()};
    }

    [[nodiscard]] auto periods() const noexcept -> std::span<counter_type const> {
        return {periods_.data(), periods_.size()};
    }

    [[nodiscard]] auto get_view() noexcept -> View { return {remaining_ticks(), periods()}; }

    [[nodiscard]] auto get_view() const noexcept -> ConstView {
        return {remaining_ticks(), periods()};
    }
  private:
    [[nodiscard]] auto to_index(size_type const index) const noexcept -> std::size_t {
        assert(static_cast<std::size_t>(index) < remaining_ticks_.size());
        return static_cast<std::size_t>(index);
    }

    std::vector<counter_type> remaining_ticks_;
    std::vector<counter_type> periods_;
};

using PeriodicTickCountdown8 = PeriodicTickCountdown<std::int8_t>;
using PeriodicTickCountdown16 = PeriodicTickCountdown<std::int16_t>;
using PeriodicTickCountdown32 = PeriodicTickCountdown<std::int32_t>;
}
