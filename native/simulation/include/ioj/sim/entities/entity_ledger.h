#pragma once

#include <ioj/sim/combat_statistics.h>
#include <ioj/sim/direct_damage_events.h>
#include <ioj/sim/entities/entity_death_info.h>
#include <ioj/sim/entities/entity_history.h>
#include <ioj/sim/entities/entity_id_allocator.h>
#include <ioj/sim/entities/health.h>

#include <array>
#include <vector>

namespace ioj::sim {
// Level-lifetime identity, history and combat accounting; no live agent storage.
class EntityLedger {
  public:
    using HistoryIndex = std::uint32_t;

    void reset();
    void reset_frame_output();
    // Require equally sized columns and output IDs. Do not borrow inputs from this ledger.
    void record_spawns(EntityType type,
                       std::span<Team const> teams,
                       std::span<Health const> healths,
                       std::span<EntityId> ids);
    void record_spawns(EntityType type,
                       std::span<Team const> teams,
                       Health health,
                       std::span<EntityId> ids);
    void record_spawns(EntityType type, Team team, Health health, std::span<EntityId> ids);
    void record_status(std::span<EntityId const> ids,
                       std::span<Team const> teams,
                       std::span<Health const> healths);
    void record_deaths(EntityDeathInfo::ConstView deaths, SimTick tick);
    // Match applied damage to the event rows; use zero for unapplied events.
    void record_damage(DirectDamageEvents::ConstView events,
                       std::span<Health const> applied_damage);
    void record_shots(std::span<EntityId const> instigators);

    // Read death metadata after record_deaths; it describes the latest death of that identity.
    auto get_unique_entities() const noexcept -> EntityHistory::ConstView {
        return history_.get_const_view();
    }
    // Borrow history rows in publication order until the next frame or ledger mutation.
    auto get_frame_deaths(EntityType type) const -> std::span<HistoryIndex const> {
        return frame_deaths_[std::to_underlying(type)];
    }
    auto get_history_index(EntityId id) const noexcept -> std::uint32_t {
        return ids_.history_index(id);
    }
    auto is_valid_unique_id(EntityId id) const noexcept -> bool {
        return get_history_index(id) != EntityIdAllocator::invalid_index;
    }
    auto get_issued_counts() const noexcept -> EntityTypeSizes const& {
        return ids_.issued_counts();
    }
    auto get_num_unique_ids_issued() const noexcept -> std::uint32_t { return history_.num(); }
    auto get_kills(EntityId id) const -> std::uint32_t;
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
    template <typename Teams, typename Healths>
    void record_spawns_impl(EntityType type,
                            Teams const& teams,
                            Healths const& healths,
                            std::span<EntityId> ids);
    auto require_history_index(EntityId id) const -> std::uint32_t;
    EntityHistory history_;
    EntityIdAllocator ids_;
    CombatStatistics statistics_;
    std::array<std::vector<HistoryIndex>, enum_count<EntityType>()> frame_deaths_;
};
}
