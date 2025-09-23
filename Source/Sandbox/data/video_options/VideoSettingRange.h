#pragma once

#include <concepts>

#include "CoreMinimal.h"

template <std::totally_ordered T>
struct SettingRange {
    using value_type = T;

    static constexpr bool has_min_max{true};

    value_type min{};
    value_type max{};

    constexpr SettingRange() = default;
    constexpr SettingRange(value_type const& min_val, value_type const& max_val)
        : min{min_val}
        , max{max_val} {}
};

template <>
struct SettingRange<bool> {
    static constexpr bool has_min_max{false};
};
