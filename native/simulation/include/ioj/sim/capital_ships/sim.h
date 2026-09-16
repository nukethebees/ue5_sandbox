#pragma once
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <format>
#include <ioj/sim/agent_accessor.h>
#include <ioj/sim/capital_spawn_data.h>
#include <ioj/sim/sim_config.h>
#include <optional>
#include <sandbox/core/countdown.h>
#include <sandbox/core/diagnostics.h>
#include <span>
#include <vector>

#include <ioj/sim/system_read_views.h>

#include <ioj/sim/capital_entity_data.h>
#include <ioj/sim/entity_death_info.h>
#include <ioj/sim/entity_types.h>
#include <ioj/sim/fighter_order_queue.h>
#include <ioj/sim/fighters/command_interface.h>
#include <ioj/sim/index_span.h>

#include <memory_resource>
#include <optional>
#include <vector>

namespace ioj::sim {
struct LevelSim;
struct CapitalShipSimConfig;
class EntityLedger;
class CombatEvents;
class LevelSpawnManager;
struct SpatialQueryManager;
}

namespace ioj::sim::fighters {
struct Sim;
}

namespace ioj::sim::capital_ships {
class PhaseInterface;

struct Sim {
    using SpawnData = CapitalSpawnData;
    using EntityData = CapitalEntityData;
    using EntityStorage = SingleAllocationCapitalEntityData;

    Sim(EntityLedger& ledger,
        CombatEvents const& combat_events,
        AgentAccessor const& agents,
        SpatialQueryManager const& spatial_query_manager,
        fighters::Sim& fighters,
        std::pmr::memory_resource& frame_memory_resource);
    Sim(Sim const&) = delete;
    Sim(Sim&&) = delete;
    auto operator=(Sim const&) -> Sim& = delete;
    auto operator=(Sim&&) -> Sim& = delete;

    /* **************************************** */
    // Configuration
    /* **************************************** */
    auto get_read_view() const -> CapitalReadView {
        return {entities.get_const_view().columns(),
                get_fighter_ids(),
                frame_changes_,
                deaths_,
                &agents_};
    }
    void reset_frame_output() {
        frame_changes_.clear();
        deaths_.clear();
    }
    void set_config(CapitalShipSimConfig const& new_config) noexcept;
    void set_diagnostics_enabled(bool enabled) noexcept { diagnostics_enabled_ = enabled; }

    /* **************************************** */
    // Accessors
    /* **************************************** */
    auto get_num_instances() const noexcept -> std::int32_t;
    auto is_valid(EntityUniqueId id) const noexcept -> bool;
    auto get_id(std::int32_t index) const -> EntityUniqueId {
        return entities.get_const_view().entity_ids()[index];
    }
    auto get_fighter_spawn_slots() const noexcept -> std::int32_t;
    auto get_fighters_spawned() const noexcept -> std::int32_t { return fighters_spawned; }
    auto get_fighter_ids() const noexcept -> std::span<EntityUniqueId const> {
        return {fighter_ids.data(), fighter_ids.size()};
    }
    auto get_fighter_id_spans() const noexcept -> std::span<IndexSpan const> {
        return entities.get_const_view().fighter_id_spans();
    }
    auto get_fighter_id_span(std::int32_t index) const noexcept -> IndexSpan {
        return entities.get_const_view().fighter_id_spans()[index];
    }
    auto get_fighter_ids(std::int32_t index) const noexcept -> std::span<EntityUniqueId const>;
    auto get_fighter_ids(IndexSpan span) const noexcept -> std::span<EntityUniqueId const>;
    auto get_target_id(std::int32_t index) const noexcept -> EntityUniqueId {
        return entities.get_const_view().target_ids()[index];
    }
    auto get_target_ids() const noexcept -> std::span<EntityUniqueId const> {
        return entities.get_const_view().target_ids();
    }
    auto get_team(std::int32_t index) const noexcept -> Team {
        return entities.get_const_view().teams()[index];
    }
    auto get_team(EntityUniqueId id) const noexcept -> Team;
    auto get_health(EntityUniqueId id) const noexcept -> Health;
    auto find_first_index_on_team(Team team) const noexcept -> std::optional<std::int32_t>;
    auto find_first_id_on_team(Team team) const noexcept -> std::optional<EntityUniqueId>;

    /* **************************************** */
    // Checks
    /* **************************************** */
    void validate_array_sizes() const;
    void set_target_id(EntityUniqueId ship_id, EntityUniqueId target_id);
  private:
    /* **************************************** */
    // Sim phases
    /* **************************************** */
    void begin_play();
    void prepare_tick(float dt);
    void think(float dt);
    void execute_fighter_self_destruct_requests();
    void resolve_damage_events();
    void resolve_fighters_of_dying_capitals();
    void publish_deaths();
    void cleanup_entities();
    void finish_action();

    /* **************************************** */
    // Ship spawning
    /* **************************************** */
    auto register_ships(CapitalSpawnDataConstView spawn_data) -> std::vector<EntityUniqueId>;
    void spawn_ships(CapitalSpawnDataConstView spawn_data);

    /* **************************************** */
    // Entity data
    /* **************************************** */

    /* **************************************** */
    // Fighter spawning
    /* **************************************** */
    void queue_fighter_spawns();
    void refresh_fighter_ids();

    /* **************************************** */
    // Orders
    /* **************************************** */
    void queue_fighter_orders();

    /* **************************************** */
    // Targets
    /* **************************************** */

    /* **************************************** */
    // Death handling
    /* **************************************** */
    void handle_dead_entities();
    void reassign_fighters_of_dying_capital();

    /* **************************************** */
    // Misc
    /* **************************************** */
    void clear_tick_buffers();

    friend class PhaseInterface;
    friend struct sim::LevelSim;

    friend class sim::LevelSpawnManager;

    CapitalShipSimConfig config{};
    bool diagnostics_enabled_{};
    EntityLedger& ledger_;
    CombatEvents const& combat_events_;
    AgentAccessor const& agents_;
    SpatialQueryManager const& spatial_query_manager;
    std::pmr::memory_resource& frame_memory_resource;

    EntityStorage entities{};
    std::vector<std::int32_t> local_indices_to_remove;
    EntityDeathInfo entity_death_info;
    std::vector<EntityFrameChange> frame_changes_;
    std::vector<CapitalDeathEvent> deaths_;

    fighters::CommandInterface fighters_interface;
    std::vector<EntityUniqueId> fighter_self_destruct_requests_;
    std::vector<EntityUniqueId> fighter_ids;
    std::int32_t fighters_spawned{0};
    std::int32_t diagnostic_spawn_reports{};

    FighterOrderQueue fighter_order_queue{};
};
} // namespace ioj::sim::capital_ships
