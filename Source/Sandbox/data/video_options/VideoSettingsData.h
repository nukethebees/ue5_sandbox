#pragma once

#include <tuple>
#include <type_traits>

#include "CoreMinimal.h"
#include "Sandbox/data/video_options/VideoRowData.h"
#include "Sandbox/data/video_options/VideoSettingConfig.h"
#include "Sandbox/data/video_options/VideoSettingRange.h"

template <template <typename...> class T>
using ExpandedSettingsT = T<BoolSettingConfig, FloatSettingConfig, IntSettingConfig>;

using VideoRow = ExpandedSettingsT<VideoRowSettingsVariantT>;
using VideoSettingsTuple = ExpandedSettingsT<VideoSettingsTupleT>;

UENUM(BlueprintType)
enum class EVideoRowSettingChangeType : uint8 { ValueChanged, ValueReset };

namespace VideoSettingsHelpers {
template <typename T>
auto& get_settings_array(VideoSettingsTuple& tuple) {
    return std::get<TArray<FVideoSettingConfig<T>>>(tuple);
}

template <typename T>
auto const& get_settings_array(VideoSettingsTuple const& tuple) {
    return std::get<TArray<FVideoSettingConfig<T>>>(tuple);
}

// Helper to create VideoRow from config
template <typename Config>
VideoRow create_row_data(Config const& config, typename Config::SettingT current_value) {
    return RowData<Config>{&config, current_value};
}
}
