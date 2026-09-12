#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace ml {
template <typename X, typename Y>
    requires std::is_arithmetic_v<X>
class XYSeriesData {
  public:
    using x_type = X;
    using y_type = Y;
    using time_type = x_type;
    using value_type = y_type;
    using size_type = std::int32_t;

    [[nodiscard]] auto num() const noexcept -> size_type {
        return static_cast<size_type>(times_.size());
    }
    [[nodiscard]] auto is_empty() const noexcept -> bool { return times_.empty(); }

    void reset() noexcept {
        times_.clear();
        values_.clear();
    }
    void reserve(size_type const count) {
        times_.reserve(static_cast<std::size_t>(count));
        values_.reserve(static_cast<std::size_t>(count));
    }

    [[nodiscard]] auto times() const noexcept -> std::span<time_type const> { return times_; }
    [[nodiscard]] auto values() const noexcept -> std::span<value_type const> { return values_; }
    [[nodiscard]] auto time_capacity() const noexcept -> size_type {
        return static_cast<size_type>(times_.capacity());
    }
    [[nodiscard]] auto value_capacity() const noexcept -> size_type {
        return static_cast<size_type>(values_.capacity());
    }
    [[nodiscard]] auto allocated_bytes() const noexcept -> std::size_t {
        return times_.capacity() * sizeof(time_type) + values_.capacity() * sizeof(value_type);
    }

    template <typename AddType>
        requires std::same_as<value_type, std::remove_cvref_t<AddType>>
    void add(time_type const t, AddType&& value) {
        assert(is_empty() || times_.back() < t);
        times_.emplace_back(t);
        values_.emplace_back(std::forward<AddType>(value));
    }

    [[nodiscard]] auto nearest_index(time_type const t) const noexcept -> size_type {
        if (is_empty()) {
            return index_none;
        }
        if (t <= times_.front()) {
            return 0;
        }

        auto const count{num()};
        for (size_type i{1}; i < count; ++i) {
            auto const index{static_cast<std::size_t>(i)};
            if (t <= times_[index]) {
                auto const previous_delta{t - times_[index - 1]};
                auto const next_delta{times_[index] - t};
                return next_delta < previous_delta ? i : i - 1;
            }
        }

        return count - 1;
    }
    [[nodiscard]] auto nearest_value(time_type const t) const -> value_type const& {
        auto const index{nearest_index(t)};
        assert(index != index_none);
        return values_[static_cast<std::size_t>(index)];
    }
    [[nodiscard]] auto nearest_time(time_type const t) const -> time_type {
        auto const index{nearest_index(t)};
        assert(index != index_none);
        return times_[static_cast<std::size_t>(index)];
    }

    [[nodiscard]] auto value_at(size_type const index) const -> value_type const& {
        return values_[static_cast<std::size_t>(index)];
    }
    [[nodiscard]] auto time_at(size_type const index) const -> time_type {
        return times_[static_cast<std::size_t>(index)];
    }

    [[nodiscard]] auto last_value() const -> value_type const& { return values_.back(); }
    [[nodiscard]] auto last_time() const -> time_type { return times_.back(); }
    [[nodiscard]] auto last_index() const noexcept -> size_type {
        return is_empty() ? index_none : num() - 1;
    }

    inline static constexpr size_type index_none{-1};
  private:
    std::vector<time_type> times_;
    std::vector<value_type> values_;
};

template <typename T>
using TimeSeriesData = XYSeriesData<double, T>;
}
