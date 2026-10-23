#pragma once

#include <ioj/constrained_numbers.h>
#include <ioj/sim/collision/collision_system.h>
#include <ioj/sim/collision/overlap_response_config.h>
#include <ioj/sim/direct_damage_events.h>
#include <ioj/sim/entities/health_type.h>

#include <cstdint>

namespace ioj::sim {
class CombatEvents;

struct OverlapHandler {
    explicit OverlapHandler(Positive<Health> damage_per_overlap_detection) noexcept;

    void handle(collision::DetectedOverlapsView overlaps, CombatEvents& events);
  private:
    void append_damage(EntityId id);

    Health damage_per_overlap_detection_{};
    DirectDamageEvents damage_events_;
};
}
