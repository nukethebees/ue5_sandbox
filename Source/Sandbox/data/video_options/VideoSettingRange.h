#pragma once

#include <concepts> 

#include "CoreMinimal.h"

template <std::totally_ordered T>
struct SettingRange {
    T min{};
    T max{};

    constexpr SettingRange() = default;
    constexpr SettingRange(T const& min_val, T const& max_val)
        : min{min_val}
        , max{max_val} {}
};

template <>
struct SettingRange<bool> {
    // No range needed for boolean settings
};