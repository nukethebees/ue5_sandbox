#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "Sandbox/concepts/concepts.h"
#include "Sandbox/data/video_options/VideoSettingRange.h"
#include <utility>

UENUM(BlueprintType)
enum class EVideoSettingType : uint8 { Checkbox, SliderWithText, TextBox };

UENUM()
enum class EVisualQualityLevel : int32 {
    Auto = -1,
    Low = 0,
    Medium = 1,
    High = 2,
    Epic = 3,
    Cinematic = 4
};

inline FText GetQualityLevelDisplayName(EVisualQualityLevel level) {
    switch (level) {
        case EVisualQualityLevel::Auto:
            return FText::FromString(TEXT("Auto"));
        case EVisualQualityLevel::Low:
            return FText::FromString(TEXT("Low"));
        case EVisualQualityLevel::Medium:
            return FText::FromString(TEXT("Medium"));
        case EVisualQualityLevel::High:
            return FText::FromString(TEXT("High"));
        case EVisualQualityLevel::Epic:
            return FText::FromString(TEXT("Epic"));
        case EVisualQualityLevel::Cinematic:
            return FText::FromString(TEXT("Cinematic"));
        default:
            return FText::FromString(TEXT("Unknown"));
    }
}

template <typename T,
          typename U = T,
          typename ValueRange = SettingRange<T>,
          typename Getter = U (UGameUserSettings::*)() const,
          typename Setter = void (UGameUserSettings::*)(U)>
struct FVideoSettingConfig {
    using SettingT = T;
    using UnrealT = U;
    using RangeT = ValueRange;

    FString setting_name{};
    EVideoSettingType type{EVideoSettingType::TextBox};
    ValueRange range{};
    Getter getter{nullptr};
    Setter setter{nullptr};

    // Call the member functions and convert to/from UnrealT if needed
    template <typename Self, typename Obj>
    decltype(auto) get_value(this Self&& self, Obj&& obj) {
        using ReturnT = decltype((std::forward<Obj>(obj).*std::forward_like<Self>(self.getter))());

        if constexpr (std::is_same_v<SettingT, ReturnT>) {
            return (std::forward<Obj>(obj).*std::forward_like<Self>(self.getter))();
        } else {
            return static_cast<SettingT>(
                (std::forward<Obj>(obj).*std::forward_like<Self>(self.getter))());
        }
    }
    template <typename Self, typename Obj, typename Value>
    void set_value(this Self&& self, Obj&& obj, Value&& value) {
        if constexpr (std::is_same_v<UnrealT, std::remove_cvref_t<Value>>) {
            (std::forward<Obj>(obj).*
             std::forward_like<Self>(self.setter))(std::forward<Value>(value));
        } else {
            (std::forward<Obj>(obj).*std::forward_like<Self>(self.setter))(
                static_cast<UnrealT>(std::forward<Value>(value)));
        }
    }
};

// Type aliases for common configurations
using BoolSettingConfig = FVideoSettingConfig<bool>;
using FloatSettingConfig = FVideoSettingConfig<float>;
using IntSettingConfig = FVideoSettingConfig<int32>;
using QualitySettingConfig = FVideoSettingConfig<EVisualQualityLevel, int32>;

template <typename... Args>
using VideoSettingsTupleT = std::tuple<TArray<Args>...>;
