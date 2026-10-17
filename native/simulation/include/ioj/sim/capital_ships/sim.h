#pragma once
#include <ioj/frame_memory_resource.h>
#include <ioj/levels/team.h>
#include <ioj/sim/capital_ships/capital_entity_data.h>
#include <ioj/sim/capital_ships/capital_ship_sim_config.h>
#include <ioj/sim/entities/entity_death_info.h>
#include <ioj/sim/entities/entity_frame_change.h>
#include <ioj/sim/entities/entity_tables.h>
#include <ioj/sim/entities/entity_type.h>
#include <ioj/sim/entities/entity_unique_id.h>
#include <ioj/sim/fighters/command_interface.h>
#include <ioj/sim/fighters/fighter_order_queue.h>
#include <ioj/sim/index_span.h>
#include <ioj/sim/levels/level_capital_spawn_events.h>
#include <ioj/sim/levels/level_mission_event_groups.h>
#include <ioj/sim/levels/level_spawn_groups.h>
#include <ioj/sim/levels/level_spinner_spawn_events.h>
#include <ioj/sim/levels/level_turret_spawn_events.h>
#include <ioj/sim/sim_clock.h>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ioj::sim {
struct LevelSim;
struct LevelSimTestAccess;
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
    using EntityStorage = CapitalEntityData;

    Sim(SimClock const& clock,
        EntityLedger& ledger,
        CombatEvents const& combat_events,
        EntityTables& entity_tables,
        SpatialQueryManager const& spatial_query_manager,
        fighters::Sim& fighters);
    Sim(Sim const&) = delete;
    Sim(Sim&&) = delete;
    auto operator=(Sim const&) -> Sim& = delete;
    auto operator=(Sim&&) -> Sim& = delete;

    /* **************************************** */
    // Configuration
    /* **************************************** */
    auto get_entities() const -> EntityStorage::ConstView { return entities.get_const_view(); }
    auto get_healths() const -> HealthConstView {
        return entity_tables_.health.get_const_view<EntityType::CapitalShip>(entities.num());
    }
    auto get_frame_changes() const -> std::span<EntityFrameChange const> { return frame_changes_; }
    void reset_frame_output() { frame_changes_.clear(); }
    void set_config(CapitalShipSimConfig const& new_config) noexcept;
    void set_diagnostics_enabled(bool enabled) noexcept { diagnostics_enabled_ = enabled; }

    /* **************************************** */
    // Accessors
    /* **************************************** */
    auto get_num_instances() const noexcept -> std::uint32_t;
    auto is_valid(EntityUniqueId id) const noexcept -> bool;
    auto get_id(std::uint32_t index) const -> EntityUniqueId {
        return entities.get_const_view().entity_ids()[index];
    }
    auto get_fighter_spawn_slots() const noexcept -> std::uint32_t;
    auto get_fighters_spawned() const noexcept -> std::uint32_t { return fighters_spawned; }
    auto get_fighter_ids() const noexcept -> std::span<EntityUniqueId const> {
        return {fighter_ids.data(), fighter_ids.size()};
    }
    auto get_fighter_id_spans() const noexcept -> std::span<IndexSpan const> {
        return entities.get_const_view().fighter_id_spans();
    }
    auto get_fighter_id_span(std::uint32_t index) const noexcept -> IndexSpan {
        return entities.get_const_view().fighter_id_spans()[index];
    }
    auto get_fighter_ids(std::uint32_t index) const noexcept -> std::span<EntityUniqueId const>;
    auto get_fighter_ids(IndexSpan span) const noexcept -> std::span<EntityUniqueId const>;
    auto get_target_id(std::uint32_t index) const noexcept -> EntityUniqueId {
        return entities.get_const_view().target_ids()[index];
    }
    auto get_target_ids() const noexcept -> std::span<EntityUniqueId const> {
        return entities.get_const_view().target_ids();
    }
    auto get_team(std::uint32_t index) const noexcept -> Team {
        return entities.get_const_view().teams()[index];
    }
    auto get_team(EntityUniqueId id) const noexcept -> Team;
    auto get_health(EntityUniqueId id) const noexcept -> Health;
    auto find_first_index_on_team(Team team) const noexcept -> std::optional<std::uint32_t>;
    auto find_first_id_on_team(Team team) const noexcept -> std::optional<EntityUniqueId>;

    /* **************************************** */
    // Checks
    /* **************************************** */
    void set_target_id(EntityUniqueId ship_id, EntityUniqueId target_id);
  private:
    /* **************************************** */
    // Sim phases
    /* **************************************** */
    void begin_play();
    void update_entity_lookup_table();
    void prepare_tick(float dt);
    void think(float dt, ml::FrameMemoryResource* const scratch_resource);
    void resolve_damage_events();
    void resolve_fighters_of_dying_capitals();
    void publish_deaths();
    void remove_components();
    void remove_entities();
    void finish_action();

    /* **************************************** */
    // Ship spawning
    /* **************************************** */
    auto register_ships(LevelCapitalSpawnEvents::ConstView spawn_data)
        -> std::vector<EntityUniqueId>;
    void spawn_ships(LevelCapitalSpawnEvents::ConstView spawn_data);

    /* **************************************** */
    // Entity data
    /* **************************************** */

    /* **************************************** */
    // Fighter spawning
    /* **************************************** */
    void queue_fighter_spawns(ml::FrameMemoryResource* const scratch_resource);
    void refresh_fighter_ids(ml::FrameMemoryResource* const scratch_resource);

    /* **************************************** */
    // Orders
    /* **************************************** */
    void queue_fighter_orders(ml::FrameMemoryResource* const scratch_resource);

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
    friend struct sim::LevelSimTestAccess;

    friend class sim::LevelSpawnManager;

    CapitalShipSimConfig config{};
    bool diagnostics_enabled_{};
    SimClock const& clock_;
    EntityLedger& ledger_;
    CombatEvents const& combat_events_;
    EntityTables& entity_tables_;
    SpatialQueryManager const& spatial_query_manager;

    EntityStorage entities{};
    std::vector<std::uint32_t> local_indices_to_remove;
    EntityDeathInfo entity_death_info;
    std::vector<EntityFrameChange> frame_changes_;

    fighters::CommandInterface fighters_interface;
    std::vector<EntityUniqueId> fighter_ids;
    std::uint64_t fighter_membership_revision_{};
    std::uint64_t fighter_layout_revision_{};
    bool fighter_ids_current_{};
    std::uint32_t fighters_spawned{0};

    FighterOrderQueue fighter_order_queue{};
};
} // namespace ioj::sim::capital_ships
