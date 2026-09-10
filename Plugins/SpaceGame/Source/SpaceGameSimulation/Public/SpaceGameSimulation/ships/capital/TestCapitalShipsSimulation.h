#pragma once
#include <SpaceGameSimulation/simulation/SystemReadViews.h>

#include <SpaceGameSimulation/simulation/LevelSimulationConfig.h>

#include <SandboxNative/RegistryEntityHandle.h>
#include <SpaceGameSimulation/entities/EntityDeathInfo.h>
#include <SpaceGameSimulation/entities/TestEntityRegistryData.h>
#include <SpaceGameSimulation/entities/TestTeam.h>
#include <SpaceGameSimulation/ships/capital/TestCapitalShipsSoA.h>
#include <SpaceGameSimulation/ships/fighters/TestCapitalShipFighterOrderQueue.h>
#include <SpaceGameSimulation/ships/fighters/TestCapitalShipFightersCommandInterface.h>
#include <SpaceGameSimulation/support/IndexSpan.h>

#include <SandboxCore/multi_buffer.h>

#include <CoreMinimal.h>

#include <memory_resource>
#include <optional>

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

struct SPACEGAMESIMULATION_API Simulation {
    using RegistryEntityData = ml::entity_registry::EntityData;
    using SpawnData = ml::test_capital_ships::SpawnData;
    using EntityTickData = ml::test_capital_ships::EntityTickData;
    using EntityData = ml::test_capital_ships::EntityData;
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
        return {
            entities.get_const_view(), &entity_registry, fighter_handles, frame_changes_, deaths_};
    }
    void reset_frame_output() {
        frame_changes_.Reset();
        deaths_.Reset();
    }
    void set_config(FCapitalSimulationConfig const& new_config) noexcept;

    /* **************************************** */
    // Accessors
    /* **************************************** */
    auto get_num_instances() const noexcept -> int32;
    auto is_valid(FRegistryEntityHandle handle) const noexcept -> bool;
    auto get_entity_registry() const noexcept -> FTestEntityRegistry const& {
        return entity_registry;
    }
    auto get_handle(int32 index) const -> FRegistryEntityHandle { return entities.handles[index]; }
    auto get_fighter_spawn_slots() const noexcept -> int32;
    auto get_fighters_spawned() const noexcept -> int32 { return fighters_spawned; }
    auto get_fighter_handles() const noexcept -> TConstArrayView<FRegistryEntityHandle> {
        return fighter_handles;
    }
    auto get_capital_fighter_handle_spans() const noexcept -> auto const& {
        return entities.capital_fighter_handle_spans;
    }
    auto get_capital_fighter_handle_span(int32 index) const noexcept -> FIndexSpan {
        return entities.capital_fighter_handle_spans[index];
    }
    auto get_fighter_handles(int32 index) const noexcept -> TConstArrayView<FRegistryEntityHandle>;
    auto get_fighter_handles(FIndexSpan span) const noexcept
        -> TConstArrayView<FRegistryEntityHandle>;
    auto get_target_handle(int32 index) const noexcept -> FRegistryEntityHandle {
        return entities.target_handles[index];
    }
    auto get_target_handles() const noexcept -> TConstArrayView<FRegistryEntityHandle> {
        return entities.target_handles;
    }
    auto get_team(int32 index) const noexcept -> ETestTeam { return entities.teams[index]; }
    auto get_team(FRegistryEntityHandle handle) const noexcept -> ETestTeam;
    auto get_health(FRegistryEntityHandle handle) const noexcept -> int32;
    auto find_first_index_on_team(ETestTeam team) const noexcept -> std::optional<int32>;
    auto find_first_handle_on_team(ETestTeam team) const noexcept
        -> std::optional<FRegistryEntityHandle>;

    /* **************************************** */
    // Checks
    /* **************************************** */
    void validate_array_sizes() const;
    void validate_entity_handles() const;
    void set_target_handle(FRegistryEntityHandle ship_handle, FRegistryEntityHandle target_handle);

    float entity_radius{0.f};
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
    auto register_ships(SpawnDataConstView spawn_data) -> TArray<FRegistryEntityHandle>;
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
    TArray<int32> local_indices_to_remove;
    EntityDeathInfo entity_death_info;
    TArray<FEntityFrameChange> frame_changes_;
    TArray<FCapitalDeathEvent> deaths_;
    RegistryEntityData entity_update_data;

    ml::test_capital_ship_fighters::CommandInterface fighters_interface;
    TArray<FRegistryEntityHandle> fighter_handles;
    TArray<FRegistryEntityHandle> fighter_handles_scratch;
    FighterReassignment fighter_reassignment_queue;
    int32 fighters_spawned{0};
    int32 diagnostic_spawn_reports{};

    TestCapitalShipFighterOrderQueue fighter_order_queue{};
};
} // namespace ml::test_capital_ships
