#pragma once

#include "CoreMinimal.h"

template <typename T>
struct SettingRange; // Forward declaration

template <>
struct SettingRange<bool> {
    // No range needed for boolean settings
};

template <>
struct SettingRange<float> {
    float min{0.0f};
    float max{1.0f};

    SettingRange() = default;
    SettingRange(float min_val, float max_val)
        : min{min_val}
        , max{max_val} {}
};

template <>
struct SettingRange<int32> {
    int32 min{0};
    int32 max{100};

    SettingRange() = default;
    SettingRange(int32 min_val, int32 max_val)
        : min{min_val}
        , max{max_val} {}
};