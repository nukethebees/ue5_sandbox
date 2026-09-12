#pragma once

#include "sandbox/core/time_series_data.h"

#include <cstdint>

namespace ml::simulation {
struct LevelTelemetrySnapshot {
    using ActiveEntityCountData = ml::XYSeriesData<std::uint64_t, std::int32_t>;
    using CumulativeKillCountData = ml::XYSeriesData<std::uint64_t, std::int32_t>;

    double elapsed_seconds{0.0};
    double tick_period{0.0};
    std::int32_t active_entities{0};
    std::int32_t spawned_entities{0};
    std::int32_t destroyed_entities{0};
    std::int32_t kills{0};
    std::int32_t active_lasers{0};
    std::int32_t lasers_fired{0};
    ActiveEntityCountData active_entity_count_data;
    CumulativeKillCountData cumulative_kill_count_data;
};
}
