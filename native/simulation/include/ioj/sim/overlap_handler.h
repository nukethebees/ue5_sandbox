#pragma once

#include <ioj/sim/collision/collision_system.h>
#include <ioj/sim/direct_damage_events.h>
#include <ioj/sim/sim_config.h>

namespace ioj::sim {
class CombatEvents;

struct OverlapHandler {
    OverlapHandler(CombatEvents& events,
                   AgentAccessor const& agents,
                   OverlapResponseConfig const& config) noexcept;

    void handle(collision::DetectedOverlapsView overlaps);
  private:
    void append_damage(EntityUniqueId id);

    CombatEvents& events_;
    AgentAccessor const& agents_;
    std::int32_t damage_per_overlap_detection_{};
    DirectDamageEvents damage_events_;
};
}
