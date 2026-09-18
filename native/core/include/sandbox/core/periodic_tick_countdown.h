#pragma once

#include "sandbox/core/countdown.h"
#include "sandbox/core/soa_permutation.h"

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace ml {
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
    using size_type = std::int32_t;
    using counter_type = Counter;
    using View = PeriodicTickCountdownView<counter_type>;
    using ConstView = PeriodicTickCountdownView<counter_type const>;

    PeriodicTickCountdown() = default;

    template <std::integral PeriodType>
    [[nodiscard]] static constexpr auto valid_period(PeriodType const period) noexcept -> bool {
        return std::in_range<counter_type>(period) && period > 0;
    }

    [[nodiscard]] auto num() const noexcept -> size_type {
        return static_cast<size_type>(remaining_ticks_.size());
    }

    [[nodiscard]] auto operator[](size_type const index) const noexcept -> counter_type {
        return remaining_ticks_[to_index(index)];
    }

    void tick() noexcept { ml::tick_periodic_countdowns(remaining_ticks()); }

    void tick(counter_type const num_ticks) noexcept {
        assert(num_ticks >= 0);
        ml::tick_periodic_countdowns(remaining_ticks(), num_ticks);
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

    template <std::integral PeriodType>
    void add_started(PeriodType const period) {
        assert(valid_period(period));
        auto const checked_period{static_cast<counter_type>(period)};
        remaining_ticks_.push_back(checked_period);
        periods_.push_back(checked_period);
    }

    void add_zeroed(counter_type const period) {
        assert(period > 0);
        remaining_ticks_.push_back(0);
        periods_.push_back(period);
    }

    template <std::integral PeriodType>
    void add_started(PeriodType const period, size_type const count) {
        assert(count >= 0);
        assert(valid_period(period));
        auto const checked_period{static_cast<counter_type>(period)};
        auto const n{static_cast<std::size_t>(count)};
        remaining_ticks_.insert(remaining_ticks_.end(), n, checked_period);
        periods_.insert(periods_.end(), n, checked_period);
    }

    void add_zeroed(counter_type const period, size_type const count) {
        assert(count >= 0);
        assert(period > 0);
        auto const n{static_cast<std::size_t>(count)};
        remaining_ticks_.insert(remaining_ticks_.end(), n, 0);
        periods_.insert(periods_.end(), n, period);
    }

    void initialise_last(counter_type const period, size_type const count) {
        assert(count >= 0);
        assert(static_cast<std::size_t>(count) <= remaining_ticks_.size());
        assert(period > 0);

        auto const first{remaining_ticks_.size() - static_cast<std::size_t>(count)};
        for (std::size_t index{first}; index < remaining_ticks_.size(); ++index) {
            remaining_ticks_[index] = 0;
            periods_[index] = period;
        }
    }

    void remove_at_swap(size_type const index, size_type const count) {
        remove_column_at_swap(remaining_ticks_, index, count);
        remove_column_at_swap(periods_, index, count);
    }

    void reserve(size_type const count) {
        assert(count >= 0);
        auto const capacity{static_cast<std::size_t>(count)};
        remaining_ticks_.reserve(capacity);
        periods_.reserve(capacity);
    }

    void add_uninitialised(size_type const count) {
        assert(count >= 0);
        auto const n{static_cast<std::size_t>(count)};
        remaining_ticks_.resize(remaining_ticks_.size() + n);
        periods_.resize(periods_.size() + n);
    }

    void add_defaulted(size_type const count) { add_uninitialised(count); }

    void set_num(size_type const count) {
        assert(count >= 0);
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
        assert(count >= 0);
        for (size_type offset{}; offset < count; ++offset) {
            copy_element(destination + offset, source, source_index + offset);
        }
    }

    void apply_permutation(std::span<std::int32_t> const indices) {
        ml::apply_permutation(remaining_ticks(), indices);
        ml::apply_permutation(periods(), indices);
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
    static void remove_column_at_swap(std::vector<counter_type>& values,
                                      size_type const index,
                                      size_type const count) {
        assert(index >= 0);
        assert(count >= 0);
        assert(static_cast<std::size_t>(index) <= values.size());
        assert(static_cast<std::size_t>(count) <= values.size() - static_cast<std::size_t>(index));

        auto const old_size{values.size()};
        auto const tail{old_size - static_cast<std::size_t>(index) -
                        static_cast<std::size_t>(count)};
        auto const moved{std::min(tail, static_cast<std::size_t>(count))};
        auto const source{old_size - moved};
        for (std::size_t offset{}; offset < moved; ++offset) {
            values[static_cast<std::size_t>(index) + offset] = values[source + offset];
        }
        values.resize(old_size - static_cast<std::size_t>(count));
    }

    [[nodiscard]] auto to_index(size_type const index) const noexcept -> std::size_t {
        assert(index >= 0);
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
