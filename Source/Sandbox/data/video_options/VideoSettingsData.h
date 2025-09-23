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

// Helper to create VideoRow from config
template <typename Config>
VideoRow create_row_data(Config const& config, typename Config::SettingT current_value) {
    return RowData<Config>{&config, current_value};
}
}
