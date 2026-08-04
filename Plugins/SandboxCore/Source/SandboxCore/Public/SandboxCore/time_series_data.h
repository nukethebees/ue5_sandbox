#pragma once

#include <Containers/Array.h>

#include <HAL/Platform.h>
#include <Math/UnrealMathUtility.h>
#include <Misc/AssertionMacros.h>
#include <Misc/CoreMiscDefines.h>

#include <type_traits>
#include <utility>

namespace ml {
template <typename T>
class TimeSeriesData {
  public:
    using time_type = double;
    using value_type = T;
    using size_type = int32;

    auto num() const -> size_type { return times_.Num(); }
    bool is_empty() const { return times_.IsEmpty(); }

    auto times() const -> TConstArrayView<time_type> { return times_; }
    auto values() const -> TConstArrayView<value_type> { return values_; }

    template <typename AddType>
        requires std::is_same_v<value_type, std::remove_cvref_t<AddType>>
    void add(time_type const t, AddType&& value) {
        if (!is_empty()) { check(times_.Last() < t); }

        times_.Emplace(t);
        values_.Emplace(std::forward<AddType>(value));
    }

    auto nearest_index(time_type const t) const -> size_type {
        if (is_empty()) { return INDEX_NONE; }

        size_type nearest_index{0};
        time_type smallest_delta{FMath::Abs(t - times_[0])};

        auto const n{num()};
        for (size_type i{1}; i < n; ++i) {
            auto const delta{t - times_[i]};
            auto const abs_delta{FMath::Abs(delta)};

            if (abs_delta < smallest_delta) {
                nearest_index = i;
                smallest_delta = abs_delta;
            }
            if (delta < time_type{0}) { break; }
        }

        return nearest_index;
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
}
