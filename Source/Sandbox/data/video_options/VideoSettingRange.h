#pragma once

#include <concepts>

#include "CoreMinimal.h"

// std::totally_ordered
template <typename T>
struct SettingRange {
    using value_type = T;

    static constexpr bool has_min_max{true};

    value_type min{};
    value_type max{};

    constexpr SettingRange() = default;
    constexpr SettingRange(value_type const& min_val, value_type const& max_val)
        : min{min_val}
        , max{max_val} {}

    constexpr T clamp(T const& value) const {
        if constexpr (std::is_same_v<T, bool>) {
            return value; // Booleans can't be clamped
        } else {
            return FMath::Clamp(value, min, max);
        }
    }

    constexpr bool within_range(T const& value) const {
        if constexpr (std::is_same_v<T, bool>) {
            return true; // Booleans are always in range
        } else {
            return value >= min && value <= max;
        }
    }

    float get_min_float() const { return static_cast<float>(min); }
    float get_max_float() const { return static_cast<float>(max); }
};

template <>
struct SettingRange<bool> {
    static constexpr bool has_min_max{false};

    constexpr bool clamp(bool const& value) const {
        return value; // Booleans can't be clamped
    }

    constexpr bool within_range(bool const& value) const {
        return true; // Booleans are always in range
    }
};
