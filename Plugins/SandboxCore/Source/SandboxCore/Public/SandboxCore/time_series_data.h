#pragma once

#include <Containers/Array.h>

#include <Containers/ArrayView.h>
#include <HAL/Platform.h>
#include <Misc/AssertionMacros.h>
#include <Misc/CoreMiscDefines.h>

#include <type_traits>
#include <utility>

namespace ml {
template <typename X, typename Y>
    requires std::is_arithmetic_v<X>
class XYSeriesData {
  public:
    using x_type = X;
    using y_type = Y;
    using time_type = x_type;
    using value_type = y_type;
    using size_type = int32;

    auto num() const -> size_type { return times_.Num(); }
    bool is_empty() const { return times_.IsEmpty(); }

    void reset() {
        times_.Reset();
        values_.Reset();
    }
    void reserve(size_type const count) {
        times_.Reserve(count);
        values_.Reserve(count);
    }

    auto times() const -> TConstArrayView<time_type> { return times_; }
    auto values() const -> TConstArrayView<value_type> { return values_; }

    template <typename AddType>
        requires std::is_same_v<value_type, std::remove_cvref_t<AddType>>
    void add(time_type const t, AddType&& value) {
        if (!is_empty()) {
            check(times_.Last() < t);
        }

        times_.Emplace(t);
        values_.Emplace(std::forward<AddType>(value));
    }

    auto nearest_index(time_type const t) const -> size_type {
        if (is_empty()) {
            return INDEX_NONE;
        }
        if (t <= times_[0]) {
            return 0;
        }

        auto const n{num()};
        for (size_type i{1}; i < n; ++i) {
            if (t <= times_[i]) {
                auto const previous_delta{t - times_[i - 1]};
                auto const next_delta{times_[i] - t};
                return next_delta < previous_delta ? i : i - 1;
            }
        }

        return n - 1;
    }
    auto nearest_value(time_type const t) const -> value_type const& {
        auto const i{nearest_index(t)};
        check(i != INDEX_NONE);
        return values_[i];
    }
    auto nearest_time(time_type const t) const -> time_type {
        auto const i{nearest_index(t)};
        check(i != INDEX_NONE);
        return times_[i];
    }

    auto value_at(size_type i) const -> value_type const& { return values_[i]; }
    auto time_at(size_type i) const -> time_type { return times_[i]; }

    auto last_value() const -> value_type const& { return values_.Last(); }
    auto last_time() const -> time_type { return times_.Last(); }
    auto last_index() const -> size_type { return is_empty() ? INDEX_NONE : num() - 1; }
  private:
    TArray<time_type> times_;
    TArray<value_type> values_;
};

template <typename T>
using TimeSeriesData = XYSeriesData<double, T>;
}
