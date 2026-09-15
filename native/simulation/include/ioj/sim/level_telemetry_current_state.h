#pragma once

#include "ioj/sim/entity_telemetry.h"

#include <cstdint>

namespace ioj::sim {
struct LevelTelemetryCurrentState {
    telemetry::EntityCounts active_entities_by_team_and_type{};
    std::int32_t active_entities{};
    std::int32_t spawned_entities{};
    std::int32_t destroyed_entities{};
    std::int32_t kills{};

    std::int32_t active_lasers{};
    std::int32_t lasers_fired{};
};
} // namespace ioj::sim
