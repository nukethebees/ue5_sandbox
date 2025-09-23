#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "Sandbox/data/video_options/VideoSettingRange.h"

UENUM(BlueprintType)
enum class EVideoSettingType : uint8 { Checkbox, SliderWithText, TextBox };

template <typename T,
    typename ValueRange = SettingRange<T>,
    typename Getter = T(UGameUserSettings:: *)() const,
    typename Setter = void (UGameUserSettings:: *)(T)>
struct FVideoSettingConfig {
    using SettingT = T;
    using RangeT = ValueRange;

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

template <typename... Args>
using VideoSettingsTupleT = std::tuple<TArray<Args>...>;