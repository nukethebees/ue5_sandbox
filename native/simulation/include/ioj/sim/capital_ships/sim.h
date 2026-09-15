#pragma once
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <format>
#include <ioj/sim/capital_spawn_data.h>
#include <ioj/sim/fighter_reassignment.h>
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

#include <sandbox/core/multi_buffer.h>

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
    using RegistryEntityData = ioj::sim::RegistryEntityData;
    using SpawnData = ioj::sim::CapitalSpawnData;
    using EntityTickData = ioj::sim::FighterSpawnQueue;
    using EntityData = ioj::sim::CapitalEntityData;
    using FighterReassignment = ioj::sim::capital_ships::FighterReassignment;
    using EntityBuffers = ml::MultiBuffer<EntityTickData, 2>;

    Sim(EntityRegistry& entity_registry,
        SpatialQueryManager const& spatial_query_manager,
        ioj::sim::fighters::Sim& fighters,
        std::pmr::memory_resource& frame_memory_resource);
    Sim(Sim const&) = delete;
    Sim(Sim&&) = delete;
    auto operator=(Sim const&) -> Sim& = delete;
    auto operator=(Sim&&) -> Sim& = delete;

    /* **************************************** */
    // Configuration
    /* **************************************** */
    auto get_read_view() const -> CapitalReadView {
        return {entities.get_const_view(),
                &entity_registry,
                get_fighter_handles(),
                frame_changes_,
                deaths_};
    }
    void reset_frame_output() {
        frame_changes_.clear();
        deaths_.clear();
    }
    void set_config(CapitalShipSimConfig const& new_config) noexcept;

    /* **************************************** */
    // Accessors
    /* **************************************** */
    auto get_num_instances() const noexcept -> std::int32_t;
    auto is_valid(RegistryEntityHandle handle) const noexcept -> bool;
    auto get_entity_registry() const noexcept -> EntityRegistry const& { return entity_registry; }
    auto get_handle(std::int32_t index) const -> RegistryEntityHandle {
        return entities.handles[index];
    }
    auto get_fighter_spawn_slots() const noexcept -> std::int32_t;
    auto get_fighters_spawned() const noexcept -> std::int32_t { return fighters_spawned; }
    auto get_fighter_handles() const noexcept -> std::span<RegistryEntityHandle const> {
        return {fighter_handles.data(), fighter_handles.size()};
    }
    auto get_fighter_handle_spans() const noexcept -> auto const& {
        return entities.fighter_handle_spans;
    }
    auto get_fighter_handle_span(std::int32_t index) const noexcept -> IndexSpan {
        return entities.fighter_handle_spans[index];
    }
    auto get_fighter_handles(std::int32_t index) const noexcept
        -> std::span<RegistryEntityHandle const>;
    auto get_fighter_handles(IndexSpan span) const noexcept
        -> std::span<RegistryEntityHandle const>;
    auto get_target_handle(std::int32_t index) const noexcept -> RegistryEntityHandle {
        return entities.target_handles[index];
    }
    auto get_target_handles() const noexcept -> std::span<RegistryEntityHandle const> {
        return entities.target_handles;
    }
    auto get_team(std::int32_t index) const noexcept -> ioj::sim::Team {
        return entities.teams[index];
    }
    auto get_team(RegistryEntityHandle handle) const noexcept -> ioj::sim::Team;
    auto get_health(RegistryEntityHandle handle) const noexcept -> std::int32_t;
    auto find_first_index_on_team(ioj::sim::Team team) const noexcept
        -> std::optional<std::int32_t>;
    auto find_first_handle_on_team(ioj::sim::Team team) const noexcept
        -> std::optional<RegistryEntityHandle>;

    /* **************************************** */
    // Checks
    /* **************************************** */
    void validate_array_sizes() const;
    void validate_entity_handles() const;
    void set_target_handle(RegistryEntityHandle ship_handle, RegistryEntityHandle target_handle);

    bool diagnostics_enabled{};
  private:
    /* **************************************** */
    // Sim phases
    /* **************************************** */
    void begin_play();
    void begin_tick();
    void update_timers(float dt);
    void make_decisions();
    void resolve_damage_events();
    void update_entity_registry();
    void sync_from_registry();
    void end_tick();

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
    void reassign_fighter_handles_of_dying_capital();

    /* **************************************** */
    // Misc
    /* **************************************** */
    void clear_tick_buffers();

    friend class PhaseInterface;
    friend struct ::ioj::sim::LevelSim;

    friend class ::ioj::sim::LevelSpawnManager;

    CapitalShipSimConfig config{};
    EntityRegistry& entity_registry;
    SpatialQueryManager const& spatial_query_manager;
    std::pmr::memory_resource& frame_memory_resource;

    EntityData entities{};
    EntityBuffers tick_buffers{};
    std::vector<std::int32_t> local_indices_to_remove;
    EntityDeathInfo entity_death_info;
    std::vector<EntityFrameChange> frame_changes_;
    std::vector<CapitalDeathEvent> deaths_;
    RegistryEntityData entity_update_data;

    ioj::sim::fighters::CommandInterface fighters_interface;
    std::vector<RegistryEntityHandle> fighter_handles;
    std::vector<RegistryEntityHandle> fighter_handles_scratch;
    FighterReassignment fighter_reassignment_queue;
    std::int32_t fighters_spawned{0};
    std::int32_t diagnostic_spawn_reports{};

    FighterOrderQueue fighter_order_queue{};
};
} // namespace ioj::sim::capital_ships
