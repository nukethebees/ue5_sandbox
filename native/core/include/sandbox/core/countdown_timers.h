#pragma once

#include "sandbox/core/countdown.h"
#include "sandbox/core/soa_permutation.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <span>
#include <vector>

namespace ml {
class CountdownTimers {
  public:
    using size_type = std::int32_t;

    void tick(float const dt) noexcept { ml::tick_countdowns(remaining_times(), dt); }

    void reset() noexcept { remaining_times_.clear(); }

    void reserve(size_type const count) {
        assert(count >= 0);
        remaining_times_.reserve(static_cast<std::size_t>(count));
    }

    void add_uninitialised(size_type const count) {
        assert(count >= 0);
        remaining_times_.resize(remaining_times_.size() + static_cast<std::size_t>(count));
    }

    void add_defaulted(size_type const count) { add_uninitialised(count); }

    void set_num(size_type const count) {
        assert(count >= 0);
        remaining_times_.resize(static_cast<std::size_t>(count));
    }

    void set(size_type const index, float const value) {
        remaining_times_[to_index(index)] = value;
    }

    [[nodiscard]] auto add(float const value) -> size_type {
        auto const index{num()};
        remaining_times_.push_back(value);
        return index;
    }

    void remove_at_swap(size_type const index, size_type const count) {
        assert(index >= 0);
        assert(count >= 0);
        assert(static_cast<std::size_t>(index) <= remaining_times_.size());
        assert(static_cast<std::size_t>(count) <=
               remaining_times_.size() - static_cast<std::size_t>(index));

        auto const old_size{remaining_times_.size()};
        auto const tail{old_size - static_cast<std::size_t>(index) -
                        static_cast<std::size_t>(count)};
        auto const moved{std::min(tail, static_cast<std::size_t>(count))};
        auto const source{old_size - moved};
        for (std::size_t offset{}; offset < moved; ++offset) {
            remaining_times_[static_cast<std::size_t>(index) + offset] =
                remaining_times_[source + offset];
        }
        remaining_times_.resize(old_size - static_cast<std::size_t>(count));
    }

    void copy_element(size_type const destination,
                      CountdownTimers const& source,
                      size_type const source_index) {
        remaining_times_[to_index(destination)] =
            source.remaining_times_[source.to_index(source_index)];
    }

    void copy_elements(size_type const destination,
                       CountdownTimers const& source,
                       size_type const source_index,
                       size_type const count) {
        assert(count >= 0);
        for (size_type offset{}; offset < count; ++offset) {
            copy_element(destination + offset, source, source_index + offset);
        }
    }

    void copy_to_tail(CountdownTimers const& source) {
        assert(num() >= source.num());
        copy_elements(num() - source.num(), source, 0, source.num());
    }

    void append(std::span<float const> const values) {
        remaining_times_.insert(remaining_times_.end(), values.begin(), values.end());
    }

    void append(CountdownTimers const& source) { append(source.remaining_times()); }

    template <typename Compare>
    void sort(Compare&& compare, std::span<std::int32_t> const scratch_indices) {
        assert(scratch_indices.size() == remaining_times_.size());
        for (std::size_t index{}; index < scratch_indices.size(); ++index) {
            scratch_indices[index] = static_cast<std::int32_t>(index);
        }
        std::sort(
            scratch_indices.begin(),
            scratch_indices.end(),
            [this, &compare](auto const lhs, auto const rhs) { return compare(*this, lhs, rhs); });
        apply_permutation(scratch_indices);
    }

    template <auto Compare>
    void sort(std::span<std::int32_t> const scratch_indices) {
        sort([](auto const& values,
                auto const lhs,
                auto const rhs) { return Compare(values, lhs, rhs); },
             scratch_indices);
    }

    void apply_permutation(std::span<std::int32_t> const indices) {
        ml::apply_permutation(remaining_times(), indices);
    }

    [[nodiscard]] auto num() const noexcept -> size_type {
        return static_cast<size_type>(remaining_times_.size());
    }

    [[nodiscard]] auto is_empty() const noexcept -> bool { return remaining_times_.empty(); }

    [[nodiscard]] auto operator[](size_type const index) const noexcept -> float {
        return remaining_times_[to_index(index)];
    }

    [[nodiscard]] auto operator[](size_type const index) noexcept -> float& {
        return remaining_times_[to_index(index)];
    }

    [[nodiscard]] auto remaining_times() noexcept -> std::span<float> {
        return {remaining_times_.data(), remaining_times_.size()};
    }

    [[nodiscard]] auto remaining_times() const noexcept -> std::span<float const> {
        return {remaining_times_.data(), remaining_times_.size()};
    }
  private:
    [[nodiscard]] auto to_index(size_type const index) const noexcept -> std::size_t {
        assert(index >= 0);
        assert(static_cast<std::size_t>(index) < remaining_times_.size());
        return static_cast<std::size_t>(index);
    }

    std::vector<float> remaining_times_;
};

class PeriodicCountdownTimers {
  public:
    using size_type = std::int32_t;

    void tick(float const dt) noexcept { ml::tick_countdowns(remaining_times(), dt); }

    [[nodiscard]] auto expired(size_type const index) const noexcept -> bool {
        return remaining_times_[to_index(index)] <= 0.f;
    }

    void reset(size_type const index) noexcept {
        auto const offset{to_index(index)};
        remaining_times_[offset] = periods_[offset];
    }

    [[nodiscard]] auto try_consume(size_type const index) noexcept -> bool {
        if (!expired(index)) {
            return false;
        }

        reset(index);
        return true;
    }

    [[nodiscard]] auto operator[](size_type const index) const noexcept -> float {
        return remaining_times_[to_index(index)];
    }

    [[nodiscard]] auto operator[](size_type const index) noexcept -> float& {
        return remaining_times_[to_index(index)];
    }

    void add_started(float const period) {
        assert(period > 0.f);
        remaining_times_.push_back(period);
        periods_.push_back(period);
    }

    void add_started(float const period, size_type const count) {
        assert(count >= 0);
        assert(period > 0.f);
        auto const n{static_cast<std::size_t>(count)};
        remaining_times_.insert(remaining_times_.end(), n, period);
        periods_.insert(periods_.end(), n, period);
    }

    void add_started(std::span<float const> const new_periods) {
        remaining_times_.insert(remaining_times_.end(), new_periods.begin(), new_periods.end());
        periods_.insert(periods_.end(), new_periods.begin(), new_periods.end());
    }

    void add_zeroed(float const period) {
        assert(period > 0.f);
        remaining_times_.push_back(0.f);
        periods_.push_back(period);
    }

    void add_zeroed(float const period, size_type const count) {
        assert(count >= 0);
        assert(period > 0.f);
        auto const n{static_cast<std::size_t>(count)};
        remaining_times_.insert(remaining_times_.end(), n, 0.f);
        periods_.insert(periods_.end(), n, period);
    }

    void add_zeroed(std::span<float const> const new_periods) {
        remaining_times_.insert(remaining_times_.end(), new_periods.size(), 0.f);
        periods_.insert(periods_.end(), new_periods.begin(), new_periods.end());
    }

    void reset() noexcept {
        remaining_times_.clear();
        periods_.clear();
    }

    void remove_at_swap(size_type const index, size_type const count) {
        remove_column_at_swap(remaining_times_, index, count);
        remove_column_at_swap(periods_, index, count);
    }

    void reserve(size_type const count) {
        assert(count >= 0);
        auto const capacity{static_cast<std::size_t>(count)};
        remaining_times_.reserve(capacity);
        periods_.reserve(capacity);
    }

    void add_uninitialised(size_type const count) {
        assert(count >= 0);
        auto const n{static_cast<std::size_t>(count)};
        remaining_times_.resize(remaining_times_.size() + n);
        periods_.resize(periods_.size() + n);
    }

    void add_defaulted(size_type const count) { add_uninitialised(count); }

    void set_num(size_type const count) {
        assert(count >= 0);
        auto const n{static_cast<std::size_t>(count)};
        remaining_times_.resize(n);
        periods_.resize(n);
    }

    void copy_element(size_type const destination,
                      PeriodicCountdownTimers const& source,
                      size_type const source_index) {
        auto const destination_index{to_index(destination)};
        auto const source_offset{source.to_index(source_index)};
        remaining_times_[destination_index] = source.remaining_times_[source_offset];
        periods_[destination_index] = source.periods_[source_offset];
    }

    void append(std::span<float const> const new_remaining_times,
                std::span<float const> const new_periods) {
        assert(new_remaining_times.size() == new_periods.size());
        remaining_times_.insert(
            remaining_times_.end(), new_remaining_times.begin(), new_remaining_times.end());
        periods_.insert(periods_.end(), new_periods.begin(), new_periods.end());
    }

    [[nodiscard]] auto num() const noexcept -> size_type {
        return static_cast<size_type>(remaining_times_.size());
    }

    [[nodiscard]] auto remaining_times() noexcept -> std::span<float> {
        return {remaining_times_.data(), remaining_times_.size()};
    }

    [[nodiscard]] auto remaining_times() const noexcept -> std::span<float const> {
        return {remaining_times_.data(), remaining_times_.size()};
    }

    [[nodiscard]] auto periods() noexcept -> std::span<float> {
        return {periods_.data(), periods_.size()};
    }

    [[nodiscard]] auto periods() const noexcept -> std::span<float const> {
        return {periods_.data(), periods_.size()};
    }
  private:
    static void remove_column_at_swap(std::vector<float>& values,
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
        assert(static_cast<std::size_t>(index) < remaining_times_.size());
        return static_cast<std::size_t>(index);
    }

    std::vector<float> remaining_times_;
    std::vector<float> periods_;
};
}
