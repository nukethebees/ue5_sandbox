#pragma once

#include <ioj/frame_memory_resource.h>
#include <ioj/sim/damage_queue.h>

namespace ioj::sim {
// Tick-local damage transport.
class CombatEvents {
  public:
    void reset() { damage_.reset(); }
    void queue_damage(DirectDamageEvents::ConstView events) { damage_.append(events); }
    void queue_damage(DirectDamageEvents const& events) { queue_damage(events.const_view()); }
    void prepare(EntityLookupTables const& indexes, FrameMemoryResource* const scratch_resource) {
        damage_.prepare(indexes, scratch_resource);
    }
    auto events_for(EntityType type) const -> DirectDamageEvents::ConstView {
        return damage_.events_for(type);
    }
    auto all_events() const -> DirectDamageEvents const& { return damage_.all_events(); }
  private:
    DamageQueue damage_;
};
}
