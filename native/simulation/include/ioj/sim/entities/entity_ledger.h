#pragma once

#include <ioj/enum_array.h>
#include <ioj/sim/combat_statistics.h>
#include <ioj/sim/entities/entity_death_info.h>
#include <ioj/sim/entities/entity_history.h>
#include <ioj/sim/entities/entity_id_allocator.h>
#include <ioj/sim/entities/health.h>

#include <vector>

namespace ioj::sim {
// Level-lifetime identity, history and combat accounting; no live agent storage.
class EntityLedger {
  public:
    using HistoryIndex = std::uint32_t;

    explicit EntityLedger(std::pmr::memory_resource& memory = *std::pmr::get_default_resource());

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

    // Read death metadata after record_deaths; it describes the latest death of that identity.
    auto unique_entities() const noexcept -> EntityHistory::ConstView {
        return history_.const_view();
    }
    // Borrow history rows in publication order until the next frame or ledger mutation.
    auto frame_deaths(EntityType type) const -> std::span<HistoryIndex const> {
        return frame_deaths_[type];
    }
    auto history_index(EntityId id) const noexcept -> std::uint32_t {
        return ids_.history_index(id);
    }
    auto is_valid_unique_id(EntityId id) const noexcept -> bool {
        return history_index(id) != EntityIdAllocator::invalid_index;
    }
    auto issued_counts() const noexcept -> EntityTypeSizes const& { return ids_.issued_counts(); }
    auto num_unique_ids_issued() const noexcept -> std::uint32_t { return history_.num(); }
    auto kills(EntityId id) const -> std::uint32_t;
    auto count_alive() const noexcept -> std::int32_t { return statistics_.alive_count(); }
    auto count_alive(EntityType type) const noexcept -> std::int32_t {
        return statistics_.count_alive(type);
    }
    auto count_kills() const noexcept -> std::int32_t {
        return statistics_.cumulative_kill_count();
    }
    auto count_alive_per_team() const noexcept -> TeamCounts {
        return statistics_.count_alive_per_team();
    }
    auto count_alive_per_team_and_type() const noexcept -> EntityCounts {
        return statistics_.count_alive_per_team_and_type();
    }
    auto count_kills(Team team) const noexcept -> std::int32_t {
        return statistics_.kill_count(team);
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
    EnumArray<std::pmr::vector<HistoryIndex>, EntityType> frame_deaths_;
};
}
