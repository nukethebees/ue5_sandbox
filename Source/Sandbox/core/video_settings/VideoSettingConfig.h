#pragma once

#include <utility>

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "Sandbox/core/video_settings/VideoSettingRange.h"
#include "Sandbox/utilities/concepts/concepts.h"

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
