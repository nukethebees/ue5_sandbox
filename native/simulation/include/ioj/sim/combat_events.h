#pragma once

#include <ioj/sim/damage_queue.h>
#include <ioj/sim/entity_ledger.h>

#include <sandbox/core/frame_memory_resource.h>

namespace ioj::sim {
// Tick-local damage transport. Owning simulations record applied damage during resolution.
class CombatEvents {
  public:
    explicit CombatEvents(EntityLedger& ledger) noexcept
        : ledger_{ledger} {}
    void reset() { damage_.reset(); }
    void queue_damage(DirectDamageEventsConstView events) {
        events.validate_array_sizes();
        damage_.append(events);
    }
    void queue_damage(DirectDamageEvents const& events) { queue_damage(events.get_const_view()); }
    void record_shots(std::span<EntityUniqueId const> instigators) {
        ledger_.record_shots(instigators);
    }
    void prepare(EntityLookupTables const& indexes,
                 ml::FrameMemoryResource* const scratch_resource) {
        damage_.prepare(indexes, scratch_resource);
    }
    auto events_for(EntityType type) const -> DirectDamageEventsConstView {
        return damage_.events_for(type);
    }
    auto all_events() const -> DirectDamageEvents const& { return damage_.all_events(); }
  private:
    EntityLedger& ledger_;
    DamageQueue damage_;
};
}
