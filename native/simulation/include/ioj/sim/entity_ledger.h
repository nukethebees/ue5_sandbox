#pragma once

#include <ioj/sim/combat_statistics.h>
#include <ioj/sim/direct_damage_events.h>
#include <ioj/sim/entity_history.h>
#include <ioj/sim/entity_id_allocator.h>

namespace ioj::sim {
// Level-lifetime identity, history and combat accounting; no live agent storage.
class EntityLedger {
  public:
    void reset();
    auto record_spawn(EntityType type, Team team, bool alive) -> EntityUniqueId;
    void record_status(EntityUniqueId id, Team team, bool alive);
    void record_death(EntityUniqueId victim, EntityUniqueId killer, DeathReason reason);
    void record_damage(DirectDamageEventsConstView events);
    void record_shots(std::span<EntityUniqueId const> instigators);

    auto get_unique_entities() const noexcept -> EntityHistoryColumnsConstView {
        return history_.get_const_view().columns();
    }
    auto get_history_index(EntityUniqueId id) const noexcept -> std::int32_t {
        return ids_.history_index(id);
    }
    auto is_valid_unique_id(EntityUniqueId id) const noexcept -> bool {
        return get_history_index(id) >= 0;
    }
    auto get_issued_counts() const noexcept -> EntityTypeSizes const& {
        return ids_.issued_counts();
    }
    auto get_num_unique_ids_issued() const noexcept -> std::int32_t { return history_.num(); }
    auto get_kills(EntityUniqueId id) const -> std::uint32_t;
    auto count_alive() const noexcept -> std::int32_t { return statistics_.alive_count(); }
    auto count_alive(EntityType type) const noexcept -> std::int32_t {
        return statistics_.count_alive(type);
    }
    auto count_kills() const noexcept -> std::int32_t {
        return statistics_.cumulative_kill_count();
    }
    auto count_alive_per_team() const noexcept -> telemetry::TeamCounts {
        return statistics_.count_alive_per_team();
    }
    auto count_alive_per_team_and_type() const noexcept -> telemetry::EntityCounts {
        return statistics_.count_alive_per_team_and_type();
    }
    auto count_alive_not_on_team(Team team) const noexcept -> std::int32_t {
        return statistics_.count_alive_not_on_team(team);
    }
    auto get_combat_telemetry() const noexcept -> telemetry::CombatTelemetryCounters const& {
        return statistics_.combat_telemetry();
    }
  private:
    auto require_history_index(EntityUniqueId id) const -> std::int32_t;
    EntityHistory history_;
    EntityIdAllocator ids_;
    CombatStatistics statistics_;
};
}
