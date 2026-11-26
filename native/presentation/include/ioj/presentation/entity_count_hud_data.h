#pragma once

#include <ioj/sim/entities/team_entity_array.h>

namespace ioj::presentation {
struct EntityCountHudData {
    auto operator==(EntityCountHudData const&) const -> bool = default;

    sim::EntityCounts alive_per_team_and_type{};
};
}
