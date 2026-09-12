#pragma once

#include <cstdint>

namespace ml::simulation::fighters {
struct NavigationTelemetrySnapshot {
    std::int32_t separating_fighter_count{};
    std::int32_t avoiding_fighter_count{};
    std::int32_t clear_risk_count{};
    std::int32_t nearby_risk_count{};
    std::int32_t active_risk_count{};
    std::int32_t immediate_risk_count{};
    std::int32_t separation_query_count{};
    std::int32_t separation_candidate_count{};
    std::int32_t dense_direction_selection_count{};
    std::int32_t steering_memory_fighter_count{};
    std::int32_t hard_trace_count{};
};
} // namespace ml::simulation::fighters
