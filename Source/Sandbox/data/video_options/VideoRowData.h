#pragma once

#include <optional>
#include <variant>

#include "CoreMinimal.h"
#include "Sandbox/concepts/concepts.h"
#include "Sandbox/data/video_options/VisualQualityLevel.h"
#include "Sandbox/mixins/LogMsgMixin.hpp"

// Row data structure for variant-based storage
template <typename Config>
struct RowData : public ml::LogMsgMixin<"RowData"> {
    using SettingT = typename Config::SettingT;
    using UnrealT = typename Config::UnrealT;
    using ConfigT = typename Config;

    FText to_display_text(SettingT const& value) const {
        if constexpr (std::is_same_v<SettingT, bool>) {
            static auto const on_text{FText::FromString(TEXT("On"))};
            static auto const off_text{FText::FromString(TEXT("Off"))};
            return value ? on_text : off_text;
        } else if constexpr (std::is_same_v<SettingT, EVisualQualityLevel>) {
            return GetQualityLevelDisplayName(value);
        } else if constexpr (ml::is_numeric<SettingT>) {
            return FText::AsNumber(value);
        } else if constexpr (std::is_enum_v<SettingT>) {
            // For other enums, show underlying value
            return FText::AsNumber(std::to_underlying(value));
        } else {
            return FText::FromString(TEXT("Unknown"));
        }
    }
  public:
    RowData() = default;
    RowData(Config const* cfg, SettingT current)
        : config{cfg}
        , current_value{current} {}

    Config const* get_config() const { return config; }
    auto has_pending_change() const { return pending_value.has_value(); }
    auto get_display_value() const { return pending_value.value_or(current_value); }

    float slider_max() const { return config ? config->range.get_max_float() : 1e9; }
    float slider_min() const { return config ? config->range.get_min_float() : -1e9; }
    float slider_pending() const { return static_cast<float>(get_display_value()); }

    void refresh_current_value() {
        if (auto* settings{UGameUserSettings::GetGameUserSettings()}) {
            if (config) {
                current_value = config->get_value(*settings);
            }
        }
    }
    template <typename T>
    void set_pending_value(T value) {
        if (!config) {
            log_error(TEXT("Config is nullptr"));
            return;
        }

        if constexpr (std::is_same_v<T, float> && !std::is_same_v<SettingT, float>) {
            // Convert float to appropriate integer type, then clamp
            auto const rounded_value{static_cast<SettingT>(FMath::RoundToInt(value))};
            auto const clamped_value{config->range.clamp(rounded_value)};
            pending_value = clamped_value;
        } else if constexpr (std::is_convertible_v<T, SettingT>) {
            // Direct assignment with clamping
            auto const converted_value{static_cast<SettingT>(value)};
            auto const clamped_value{config->range.clamp(converted_value)};
            pending_value = clamped_value;
        } else {
            // Invalid type - log error and ignore
            log_error(TEXT("Invalid type passed to set_pending_value"));
        }
    }
    void write_pending_value_to_process() {
        if (pending_value && config) {
            if (auto* settings{UGameUserSettings::GetGameUserSettings()}) {
                log_verbose(TEXT("Writing setting for %s"), *config->setting_name);

                current_value = *pending_value;
                config->set_value(*settings, current_value);
                pending_value = std::nullopt;
            }
        }
    }
    void reset_pending_value() { pending_value = std::nullopt; }

    FText get_pending_display_text() const { return to_display_text(get_display_value()); }
    FText get_current_display_text() const { return to_display_text(current_value); }
  private:
    Config const* config{nullptr};
    SettingT current_value{};
    std::optional<SettingT> pending_value{std::nullopt};
};

template <typename... Args>
using VideoRowSettingsVariantT = std::variant<RowData<Args>...>;
