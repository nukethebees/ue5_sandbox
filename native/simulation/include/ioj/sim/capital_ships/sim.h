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
#include <ioj/sim/entity_handle.h>
#include <ioj/sim/entity_types.h>
#include <ioj/sim/fighter_order_queue.h>
#include <ioj/sim/fighters/command_interface.h>
#include <ioj/sim/index_span.h>
#include <ioj/sim/registry_entity_data.h>

#include <memory_resource>
#include <optional>
#include <vector>

namespace ioj::sim {
struct LevelSim;
struct CapitalShipSimConfig;
struct EntityRegistry;
class LevelSpawnManager;
struct SpatialQueryManager;
}

namespace ioj::sim::fighters {
struct Sim;
}

namespace ioj::sim::capital_ships {
class PhaseInterface;

struct Sim {
    using RegistryEntityData = sim::RegistryEntityData;
    using SpawnData = CapitalSpawnData;
    using EntityData = CapitalEntityData;
    using EntityStorage = SingleAllocationCapitalEntityData;

    Sim(EntityRegistry& entity_registry,
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
                &entity_registry,
                get_fighter_handles(),
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
    auto is_valid(RegistryEntityHandle handle) const noexcept -> bool;
    auto get_entity_registry() const noexcept -> EntityRegistry const& { return entity_registry; }
    auto get_handle(std::int32_t index) const -> RegistryEntityHandle {
        return entities.get_const_view().handles()[index];
    }
    auto get_fighter_spawn_slots() const noexcept -> std::int32_t;
    auto get_fighters_spawned() const noexcept -> std::int32_t { return fighters_spawned; }
    auto get_fighter_handles() const noexcept -> std::span<RegistryEntityHandle const> {
        return {fighter_handles.data(), fighter_handles.size()};
    }
    auto get_fighter_handle_spans() const noexcept -> std::span<IndexSpan const> {
        return entities.get_const_view().fighter_handle_spans();
    }
    auto get_fighter_handle_span(std::int32_t index) const noexcept -> IndexSpan {
        return entities.get_const_view().fighter_handle_spans()[index];
    }
    auto get_fighter_handles(std::int32_t index) const noexcept
        -> std::span<RegistryEntityHandle const>;
    auto get_fighter_handles(IndexSpan span) const noexcept
        -> std::span<RegistryEntityHandle const>;
    auto get_target_id(std::int32_t index) const noexcept -> EntityUniqueId {
        return entities.get_const_view().target_ids()[index];
    }
    auto get_target_ids() const noexcept -> std::span<EntityUniqueId const> {
        return entities.get_const_view().target_ids();
    }
    auto get_team(std::int32_t index) const noexcept -> Team {
        return entities.get_const_view().teams()[index];
    }
    auto get_team(RegistryEntityHandle handle) const noexcept -> Team;
    auto get_health(RegistryEntityHandle handle) const noexcept -> Health;
    auto find_first_index_on_team(Team team) const noexcept -> std::optional<std::int32_t>;
    auto find_first_handle_on_team(Team team) const noexcept -> std::optional<RegistryEntityHandle>;

    /* **************************************** */
    // Checks
    /* **************************************** */
    void validate_array_sizes() const;
    void validate_entity_handles() const;
    void set_target_id(RegistryEntityHandle ship_handle, EntityUniqueId target_id);
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
    void update_entity_registry();
    void cleanup_entities();
    void finish_action();

    /* **************************************** */
    // Ship spawning
    /* **************************************** */
    auto register_ships(CapitalSpawnDataConstView spawn_data) -> std::vector<RegistryEntityHandle>;
    void spawn_ships(CapitalSpawnDataConstView spawn_data);

    /* **************************************** */
    // Entity data
    /* **************************************** */
    void prepare_entity_update_data();

    /* **************************************** */
    // Fighter spawning
    /* **************************************** */
    void queue_fighter_spawns();
    void refresh_fighter_handles();

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
    EntityRegistry& entity_registry;
    AgentAccessor const& agents_;
    SpatialQueryManager const& spatial_query_manager;
    std::pmr::memory_resource& frame_memory_resource;

    EntityStorage entities{};
    std::vector<std::int32_t> local_indices_to_remove;
    EntityDeathInfo entity_death_info;
    std::vector<EntityFrameChange> frame_changes_;
    std::vector<CapitalDeathEvent> deaths_;
    SingleAllocationRegistryEntityData entity_update_data;

    fighters::CommandInterface fighters_interface;
    std::vector<EntityUniqueId> fighter_self_destruct_requests_;
    std::vector<RegistryEntityHandle> fighter_handles;
    std::int32_t fighters_spawned{0};
    std::int32_t diagnostic_spawn_reports{};

    FighterOrderQueue fighter_order_queue{};
};
} // namespace ioj::sim::capital_ships
