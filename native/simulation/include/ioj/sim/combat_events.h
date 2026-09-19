#pragma once

#include <ioj/sim/damage_queue.h>
#include <ioj/sim/entity_ledger.h>

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
    void prepare(AgentIndexes const& indexes, ml::FrameScratch& scratch) {
        damage_.prepare(indexes, scratch);
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
