#pragma once
#include "ioj/sim/sim_tick.h"
#include "ioj/time_series_data.h"
#include <ioj/duration.h>
#include <ioj/frequency.h>

#include <cstdint>

namespace ioj::sim {
struct LevelTelemetrySnapshot {
    using ActiveEntityCountData = XYSeriesData<SimTick::value_type, std::int32_t>;
    using CumulativeKillCountData = XYSeriesData<SimTick::value_type, std::int32_t>;

    Duration elapsed_seconds{};
    Frequency tick_rate_hz{Frequency::make(60.0)};
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
