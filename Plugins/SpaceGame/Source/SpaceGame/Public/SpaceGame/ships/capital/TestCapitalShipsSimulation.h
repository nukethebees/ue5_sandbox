#pragma once

#include <SpaceGame/simulation/LevelSimulationConfig.h>

#include <SandboxNative/RegistryEntityHandle.h>
#include <SpaceGame/entities/EntityDeathInfo.h>
#include <SpaceGame/entities/TestEntityRegistryData.h>
#include <SpaceGame/entities/TestTeam.h>
#include <SpaceGame/ships/capital/TestCapitalShipsSoA.h>
#include <SpaceGame/ships/fighters/TestCapitalShipFighterOrderQueue.h>
#include <SpaceGame/ships/fighters/TestCapitalShipFightersCommandInterface.h>
#include <SpaceGame/support/IndexSpan.h>

#include <SandboxCore/multi_buffer.h>

#include <CoreMinimal.h>

#include <optional>

struct FCapitalPresentation;
class ATestBatchOrchestrator;
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

struct SPACEGAME_API Simulation {
    using RegistryEntityData = ml::entity_registry::EntityData;
    using SpawnData = ml::test_capital_ships::SpawnData;
    using EntityTickData = ml::test_capital_ships::EntityTickData;
    using EntityData = ml::test_capital_ships::EntityData;
    using FighterReassignment = ml::test_capital_ships::FighterReassignment;
    using EntityBuffers = ml::MultiBuffer<EntityTickData, 2>;

    void set_config(FCapitalSimulationConfig const& new_config) noexcept;
    void set_entity_registry(FTestEntityRegistry& new_entity_registry) noexcept;
    void set_spatial_query_manager(FSpatialQueryManager const& new_query_manager) noexcept;
    void bind_fighters(ml::test_capital_ship_fighters::Simulation& fighters);

    auto get_num_instances() const noexcept -> int32;
    auto is_valid(FRegistryEntityHandle handle) const noexcept -> bool;
    auto get_entity_registry() const noexcept -> FTestEntityRegistry const* {
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

    void validate_array_sizes() const;
    void validate_proxy_handles() const;

    float entity_radius{0.f};
  private:
    void begin_play();
    void begin_tick();
    void update_timers(float dt);
    void make_decisions();
    void resolve_damage_events();
    void update_entity_registry();
    void sync_from_registry();
    void end_tick();

    auto register_ships(SpawnDataConstView spawn_data) -> TArray<FRegistryEntityHandle>;
    void set_target_handle(FRegistryEntityHandle ship_handle, FRegistryEntityHandle target_handle);
    void spawn_ships(SpawnDataConstView spawn_data);
    void prepare_entity_update_data();
    void queue_fighter_spawns();
    void refresh_fighter_handles();
    void queue_fighter_orders();
    void handle_dead_entities();
    void reassign_fighter_handles_of_dying_capital();
    void clear_tick_buffers();
    void clear_presentation_events();

    friend class PhaseInterface;
    friend class ::ATestBatchOrchestrator;
    friend struct ::FLevelSimulation;
    friend struct ::FCapitalPresentation;
    friend class ::ml::FLevelSpawnManager;

    FCapitalSimulationConfig config{};
    FTestEntityRegistry* entity_registry{nullptr};
    FSpatialQueryManager const* spatial_query_manager{nullptr};

    EntityData entities{};
    EntityBuffers tick_buffers{};
    TArray<int32> local_indices_to_remove;
    EntityDeathInfo entity_death_info;
    RegistryEntityData entity_update_data;

    ml::test_capital_ship_fighters::CommandInterface fighters_interface;
    TArray<FRegistryEntityHandle> fighter_handles;
    TArray<FRegistryEntityHandle> fighter_handles_scratch;
    FighterReassignment fighter_reassignment_queue;
    int32 fighters_spawned{0};

    TArray<int32> indices_without_targets_buffer;
    TestCapitalShipFighterOrderQueue fighter_order_queue{};

    TArray<int32> presentation_indices_to_remove;
    TArray<FVector3f> presentation_death_locations;
    int32 presentation_spawn_start{};
    int32 presentation_spawn_count{};
};
} // namespace ml::test_capital_ships
