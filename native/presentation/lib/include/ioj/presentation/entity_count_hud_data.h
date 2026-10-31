#pragma once

#include <ioj/sim/entities/entity_telemetry.h>

namespace ioj::presentation {
struct EntityCountHudData {
    auto operator==(EntityCountHudData const&) const -> bool = default;

    sim::telemetry::EntityCounts alive_per_team_and_type{};
};
}
