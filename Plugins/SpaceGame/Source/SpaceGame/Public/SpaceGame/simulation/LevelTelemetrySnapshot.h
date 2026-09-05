#pragma once

#include <SandboxCore/time_series_data.h>

#include <HAL/Platform.h>

struct SPACEGAME_API FLevelTelemetrySnapshot {
    using ActiveEntityCountData = ml::XYSeriesData<uint64, int32>;
    using CumulativeKillCountData = ml::XYSeriesData<uint64, int32>;

    double elapsed_seconds{0.0};
    double tick_period{0.0};
    int32 active_entities{0};
    int32 spawned_entities{0};
    int32 destroyed_entities{0};
    int32 kills{0};
    int32 active_lasers{0};
    int32 lasers_fired{0};
    ActiveEntityCountData active_entity_count_data;
    CumulativeKillCountData cumulative_kill_count_data;
};
