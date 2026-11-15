#pragma once

#include <ioj/enum_array.h>
#include <ioj/sim/combat_statistics.h>
#include <ioj/sim/entities/entity_death_info.h>
#include <ioj/sim/entities/entity_history.h>
#include <ioj/sim/entities/entity_identity_layout.h>
#include <ioj/sim/entities/health.h>

#include <cassert>
#include <limits>
#include <memory_resource>
#include <vector>

namespace ioj::sim {
// Record lifetime identities. Accumulate combat history.
class EntityLedger {
  public:
    using HistoryIndex = std::uint32_t;
    static constexpr HistoryIndex invalid_history_index{std::numeric_limits<HistoryIndex>::max()};

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
    void record_deaths(EntityDeathInfo::CompactConstView deaths, SimTick tick);

    // Index issued identities with history_index() or frame_deaths(). Unissued slots are unused.
    // Death metadata describes the latest death recorded for that identity.
    auto history() const noexcept -> EntityHistory::CompactConstView {
        return history_.const_view();
    }
    // Read issued entities of one type. Index with EntityId::index().
    auto unique_entities(EntityType type) const noexcept -> EntityHistory::CompactConstView {
        assert(is_valid(type));
        return history_.const_view(entity_identity_offsets[type], issued_counts_[type]);
    }
    // Borrow history rows in publication order until the next frame or ledger mutation.
    auto frame_deaths(EntityType type) const -> std::span<HistoryIndex const> {
        return frame_deaths_[type];
    }
    auto history_index(EntityId id) const noexcept -> std::uint32_t {
        return is_valid_unique_id(id) ? entity_identity_offsets[id.entity_type()] + id.index()
                                      : invalid_history_index;
    }
    auto is_valid_unique_id(EntityId id) const noexcept -> bool {
        return id.is_valid() && id.index() < issued_counts_[id.entity_type()];
    }
    auto issued_counts() const noexcept -> EntityTypeSizes const& { return issued_counts_; }
    auto kills(EntityId id) const -> std::uint32_t;
    auto count_alive() const noexcept -> std::int32_t { return statistics_.alive_count(); }
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

    // Entity ID -> type offset + ordinal -> fixed history row.
    EntityHistory history_;
    EntityTypeSizes issued_counts_{};
    CombatStatistics statistics_;
    EnumArray<std::pmr::vector<HistoryIndex>, EntityType> frame_deaths_;
};
}
