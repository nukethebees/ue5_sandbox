#pragma once

#include "sandbox/simulation/entity_telemetry.h"

#include <cstdint>

namespace ml::simulation {
struct LevelTelemetryCurrentState {
    telemetry::EntityCounts active_entities_by_team_and_type{};
    std::int32_t active_entities{};
    std::int32_t spawned_entities{};
    std::int32_t destroyed_entities{};
    std::int32_t kills{};

    std::int32_t registry_slot_count{};

    std::int32_t active_lasers{};
    std::int32_t lasers_fired{};

    std::int32_t occupied_spatial_cell_count{};
    std::uint64_t grid_rebuild_count{};
    std::uint64_t range_query_count{};
    std::uint64_t line_trace_count{};
    std::uint64_t sweep_trace_count{};
};
} // namespace ml::simulation
