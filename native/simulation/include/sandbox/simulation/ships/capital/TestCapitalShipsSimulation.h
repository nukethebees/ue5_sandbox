#pragma once
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <format>
#include <optional>
#include <sandbox/core/countdown.h>
#include <sandbox/core/diagnostics.h>
#include <sandbox/simulation/fighter_reassignment.h>
#include <sandbox/simulation/ships/capital/TestCapitalShipsSpawnData.h>
#include <sandbox/simulation/simulation/LevelSimulationConfig.h>
#include <span>
#include <vector>

#include <sandbox/simulation/simulation/SystemReadViews.h>

#include <sandbox/simulation/capital_entity_data.h>
#include <sandbox/simulation/entity_death_info.h>
#include <sandbox/simulation/entity_handle.h>
#include <sandbox/simulation/entity_types.h>
#include <sandbox/simulation/fighter_order_queue.h>
#include <sandbox/simulation/index_span.h>
#include <sandbox/simulation/registry_entity_data.h>
#include <sandbox/simulation/ships/fighters/TestCapitalShipFightersCommandInterface.h>

#include <sandbox/core/multi_buffer.h>

#include <memory_resource>
#include <optional>
#include <vector>

struct FLevelSimulation;
struct FCapitalSimulationConfig;
struct FTestEntityRegistry;

namespace ml {
class FLevelSpawnManager;
struct FSpatialQueryManager;
}

namespace ml::test_capital_ship_fighters {
struct Simulation;
}

namespace ml::test_capital_ships {
class PhaseInterface;

struct Simulation {
    using RegistryEntityData = ml::simulation::RegistryEntityData;
    using SpawnData = ml::test_capital_ships::SpawnData;
    using EntityTickData = ml::simulation::TestCapitalShipFighterSpawnQueue;
    using EntityData = ml::simulation::CapitalEntityData;
    using FighterReassignment = ml::test_capital_ships::FighterReassignment;
    using EntityBuffers = ml::MultiBuffer<EntityTickData, 2>;

    Simulation(FTestEntityRegistry& entity_registry,
               FSpatialQueryManager const& spatial_query_manager,
               ml::test_capital_ship_fighters::Simulation& fighters,
               std::pmr::memory_resource& frame_memory_resource);
    Simulation(Simulation const&) = delete;
    Simulation(Simulation&&) = delete;
    auto operator=(Simulation const&) -> Simulation& = delete;
    auto operator=(Simulation&&) -> Simulation& = delete;

    /* **************************************** */
    // Configuration
    /* **************************************** */
    auto get_read_view() const -> FCapitalReadView {
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
    void set_config(FCapitalSimulationConfig const& new_config) noexcept;

    /* **************************************** */
    // Accessors
    /* **************************************** */
    auto get_num_instances() const noexcept -> std::int32_t;
    auto is_valid(FRegistryEntityHandle handle) const noexcept -> bool;
    auto get_entity_registry() const noexcept -> FTestEntityRegistry const& {
        return entity_registry;
    }
    auto get_handle(std::int32_t index) const -> FRegistryEntityHandle {
        return entities.handles[index];
    }
    auto get_fighter_spawn_slots() const noexcept -> std::int32_t;
    auto get_fighters_spawned() const noexcept -> std::int32_t { return fighters_spawned; }
    auto get_fighter_handles() const noexcept -> std::span<FRegistryEntityHandle const> {
        return {fighter_handles.data(), fighter_handles.size()};
    }
    auto get_capital_fighter_handle_spans() const noexcept -> auto const& {
        return entities.capital_fighter_handle_spans;
    }
    auto get_capital_fighter_handle_span(std::int32_t index) const noexcept -> FIndexSpan {
        return entities.capital_fighter_handle_spans[index];
    }
    auto get_fighter_handles(std::int32_t index) const noexcept
        -> std::span<FRegistryEntityHandle const>;
    auto get_fighter_handles(FIndexSpan span) const noexcept
        -> std::span<FRegistryEntityHandle const>;
    auto get_target_handle(std::int32_t index) const noexcept -> FRegistryEntityHandle {
        return entities.target_handles[index];
    }
    auto get_target_handles() const noexcept -> std::span<FRegistryEntityHandle const> {
        return entities.target_handles;
    }
    auto get_team(std::int32_t index) const noexcept -> ml::simulation::Team {
        return entities.teams[index];
    }
    auto get_team(FRegistryEntityHandle handle) const noexcept -> ml::simulation::Team;
    auto get_health(FRegistryEntityHandle handle) const noexcept -> std::int32_t;
    auto find_first_index_on_team(ml::simulation::Team team) const noexcept
        -> std::optional<std::int32_t>;
    auto find_first_handle_on_team(ml::simulation::Team team) const noexcept
        -> std::optional<FRegistryEntityHandle>;

    /* **************************************** */
    // Checks
    /* **************************************** */
    void validate_array_sizes() const;
    void validate_entity_handles() const;
    void set_target_handle(FRegistryEntityHandle ship_handle, FRegistryEntityHandle target_handle);

    bool diagnostics_enabled{};
  private:
    /* **************************************** */
    // Simulation phases
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
    auto register_ships(SpawnDataConstView spawn_data) -> std::vector<FRegistryEntityHandle>;
    void spawn_ships(SpawnDataConstView spawn_data);

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
    friend struct ::FLevelSimulation;

    friend class ::ml::FLevelSpawnManager;

    FCapitalSimulationConfig config{};
    FTestEntityRegistry& entity_registry;
    FSpatialQueryManager const& spatial_query_manager;
    std::pmr::memory_resource& frame_memory_resource;

    EntityData entities{};
    EntityBuffers tick_buffers{};
    std::vector<std::int32_t> local_indices_to_remove;
    EntityDeathInfo entity_death_info;
    std::vector<FEntityFrameChange> frame_changes_;
    std::vector<FCapitalDeathEvent> deaths_;
    RegistryEntityData entity_update_data;

    ml::test_capital_ship_fighters::CommandInterface fighters_interface;
    std::vector<FRegistryEntityHandle> fighter_handles;
    std::vector<FRegistryEntityHandle> fighter_handles_scratch;
    FighterReassignment fighter_reassignment_queue;
    std::int32_t fighters_spawned{0};
    std::int32_t diagnostic_spawn_reports{};

    TestCapitalShipFighterOrderQueue fighter_order_queue{};
};
} // namespace ml::test_capital_ships
