#pragma once

#include <tuple>
#include <type_traits>
#include <utility>

#include "CoreMinimal.h"
#include "Sandbox/core/video_settings/VideoRowData.h"
#include "Sandbox/core/video_settings/VideoSettingConfig.h"
#include "Sandbox/core/video_settings/VideoSettingRange.h"

template <template <typename...> class T>
using ExpandedSettingsT =
    T<BoolSettingConfig, FloatSettingConfig, IntSettingConfig, QualitySettingConfig>;

using VideoRow = ExpandedSettingsT<VideoRowSettingsVariantT>;
using VideoSettingsTuple = ExpandedSettingsT<VideoSettingsTupleT>;

UENUM(BlueprintType)
enum class EVideoRowSettingChangeType : uint8 { ValueChanged, ValueReset };

namespace VideoSettingsHelpers {
template <typename T, typename Tuple>
auto&& get_settings_array(Tuple&& tuple) {
    return std::get<TArray<FVideoSettingConfig<T>>>(std::forward<Tuple>(tuple));
}

template <typename ConfigT, typename Tuple>
auto&& get_config_settings_array(Tuple&& tuple) {
    return std::get<TArray<ConfigT>>(std::forward<Tuple>(tuple));
}
}
