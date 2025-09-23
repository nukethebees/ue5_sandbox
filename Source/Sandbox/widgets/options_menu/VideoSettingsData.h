#pragma once

#include <optional>
#include <tuple>
#include <variant>

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

template <template <typename...> class T>
using ExpandedSettingsT = T<BoolSettingConfig, FloatSettingConfig, IntSettingConfig>;

template <typename... Args>
using VideoSettingsTupleT = std::tuple<TArray<Args>...>;

// Row data structure for variant-based storage
template <typename Config>
struct RowData {
    using SettingT = typename Config::SettingT;

    Config const* config{nullptr};
    SettingT current_value{};
    std::optional<SettingT> pending_value{std::nullopt};

    RowData() = default;
    RowData(Config const* cfg, SettingT current)
        : config{cfg}
        , current_value{current} {}

    bool has_pending_change() const { return pending_value.has_value(); }
    void set_pending_value(SettingT value) { pending_value = value; }
    void reset_pending() { pending_value = std::nullopt; }
    SettingT get_display_value() const { return pending_value.value_or(current_value); }
};

template <typename... Args>
using VideoRowSettingsVariantT = std::variant<RowData<Args>...>;

// Variant for type-safe row storage
using RowVariant = ExpandedSettingsT<VideoRowSettingsVariantT>;

// Config type IDs for std::get operations
enum class ConfigTypeId : int32 { Bool = 0, Float = 1, Int = 2 };

// Setting change types
UENUM(BlueprintType)
enum class ESettingChangeType : uint8 { ValueChanged, ValueReset };

// Tuple of arrays for storing different setting types
using VideoSettingsTuple = ExpandedSettingsT<VideoSettingsTupleT>;

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

// Config type identification
template <typename Config>
constexpr ConfigTypeId get_config_type_id() {
    if constexpr (std::is_same_v<Config, BoolSettingConfig>) {
        return ConfigTypeId::Bool;
    } else if constexpr (std::is_same_v<Config, FloatSettingConfig>) {
        return ConfigTypeId::Float;
    } else if constexpr (std::is_same_v<Config, IntSettingConfig>) {
        return ConfigTypeId::Int;
    } else {
        static_assert(std::is_same_v<Config, void>, "Unsupported config type");
    }
}

// Helper to create RowVariant from config
template <typename Config>
RowVariant create_row_data(Config const& config, typename Config::SettingT current_value) {
    return RowData<Config>{&config, current_value};
}
}
