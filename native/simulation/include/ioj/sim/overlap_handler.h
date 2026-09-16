#pragma once

#include <ioj/sim/collision/collision_system.h>
#include <ioj/sim/direct_damage_events.h>
#include <ioj/sim/sim_config.h>

namespace ioj::sim {
struct EntityRegistry;

struct OverlapHandler {
    OverlapHandler(EntityRegistry& registry,
                   AgentAccessor const& agents,
                   OverlapResponseConfig const& config) noexcept;

    void handle(collision::DetectedOverlapsView overlaps);
  private:
    void append_damage(RegistryEntityHandle entity);

    EntityRegistry& registry_;
    AgentAccessor const& agents_;
    std::int32_t damage_per_overlap_detection_{};
    DirectDamageEvents damage_events_;
};
}
