#pragma once

#include <tuple>

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"

UENUM(BlueprintType)
enum class EVideoSettingType : uint8 { Checkbox, SliderWithText, TextBox };

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

template <typename T,
          typename ValueRange = SettingRange<T>,
          typename Getter = T (UGameUserSettings::*)() const,
          typename Setter = void (UGameUserSettings::*)(T)>
struct FVideoSettingConfig {
    using SettingT = T;

    FString setting_name{};
    EVideoSettingType type{EVideoSettingType::TextBox};
    ValueRange range{};
    Getter getter{nullptr};
    Setter setter{nullptr};
};

// Type aliases for common configurations
using BoolSettingConfig = FVideoSettingConfig<bool>;
using FloatSettingConfig = FVideoSettingConfig<float>;
using IntSettingConfig = FVideoSettingConfig<int32>;

// Tuple of arrays for storing different setting types
using VideoSettingsTuple =
    std::tuple<TArray<BoolSettingConfig>, TArray<FloatSettingConfig>, TArray<IntSettingConfig>>;

// Helper template for accessing arrays from tuple
namespace VideoSettingsHelpers {
template <typename T>
constexpr auto get_array_index() {
    if constexpr (std::is_same_v<T, bool>) {
        return 0;
    } else if constexpr (std::is_same_v<T, float>) {
        return 1;
    } else if constexpr (std::is_same_v<T, int32>) {
        return 2;
    } else {
        static_assert(std::is_same_v<T, void>, "Unsupported setting type");
    }
}

template <typename T>
auto& get_settings_array(VideoSettingsTuple& tuple) {
    return std::get<get_array_index<T>()>(tuple);
}

template <typename T>
auto const& get_settings_array(VideoSettingsTuple const& tuple) {
    return std::get<get_array_index<T>()>(tuple);
}
}
