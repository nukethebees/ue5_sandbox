#pragma once

#include <tuple>
#include <type_traits>
#include <utility>

#include "CoreMinimal.h"
#include "Sandbox/data/video_options/VideoRowData.h"
#include "Sandbox/data/video_options/VideoSettingConfig.h"
#include "Sandbox/data/video_options/VideoSettingRange.h"

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
auto && get_config_settings_array(Tuple && tuple) {
    return std::get<TArray<ConfigT>>(std::forward<Tuple>(tuple));
}

// Helper to create VideoRow from config
template <typename Config>
VideoRow create_row_data(Config const& config, typename Config::SettingT current_value) {
    return RowData<Config>{&config, current_value};
}
}
