#pragma once

#include <ioj/presentation/top_killer_entry.h>
#include <ioj/sim/entities/entity_telemetry.h>

#include <vector>

namespace ioj::presentation {
struct KillHudData {
    auto operator==(KillHudData const&) const -> bool = default;

    std::vector<TopKillerEntry> top_killers;
    sim::telemetry::EntityCounts team_kill_matrix{};
};
}
