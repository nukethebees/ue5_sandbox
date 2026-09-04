#pragma once

#include <SpaceGame/combat/lasers/TestLasersSimulation.h>
#include <SpaceGame/defences/turrets/TestStaticTurretsSoA.h>
#include <SpaceGame/entities/EntityDeathInfo.h>
#include <SpaceGame/entities/TestEntityRegistryData.h>
#include <SpaceGame/simulation/SimulationClockInterface.h>

#include <CoreMinimal.h>

class ATestBatchOrchestrator;
class ATestStaticTurrets;
struct FTurretConfig;
struct FTestEntityRegistry;

namespace ml {
struct FSpatialQueryManager;
}

namespace ml::test_static_turrets {
class PhaseInterface;

struct SPACEGAME_API Simulation {
    using RegistryEntityData = ml::entity_registry::EntityData;
    using EntityData = ml::test_static_turrets::EntityData;
    using SpawnData = ml::test_static_turrets::SpawnData;

    void set_config(FTurretConfig const& new_config) noexcept;
    void bind_simulation_clock(ATestBatchOrchestrator const& orchestrator) noexcept;
    void set_entity_registry(FTestEntityRegistry& new_registry) noexcept;
    void set_spatial_query_manager(FSpatialQueryManager const& manager) noexcept;
    void set_laser_simulation(ml::test_lasers::Simulation& new_simulation) noexcept;

    auto get_num_instances() const noexcept -> int32;
    auto get_target_handles() const -> TConstArrayView<FRegistryEntityHandle>;
    auto get_entity_registry() const -> FTestEntityRegistry const* { return entity_registry; }
    auto get_laser_simulation() const -> ml::test_lasers::Simulation const* {
        return laser_simulation;
    }
    void validate_array_sizes() const;
    void validate_proxy_handles() const;

    float entity_radius{0.f};
    int32 search_slice_size{64};
  private:
    void clear_runtime_state();
    void begin_play();
    void begin_tick();
    void update_timers(float dt);
    void make_decisions();
    void queue_commands();
    void resolve_damage_events();
    void update_entity_registry();
    void sync_from_registry();
    void end_tick();

    void register_turrets(SpawnData const& spawn_data);
    void prepare_entity_update_data();
    void perform_search();
    void perform_search_on_slice(int32 job_index,
                                 int32 n_turrets,
                                 int32 turrets_per_job,
                                 float radius);
    void fire_at_enemies();
    auto get_disengage_radius() const -> float;
    void handle_dead_entities();
    void clear_tick_buffers();

    friend class PhaseInterface;
    friend class ::ATestBatchOrchestrator;
    friend class ::ATestStaticTurrets;

    FTurretConfig const* config{nullptr};
    ml::test_batch_orchestrator::SimulationClockInterface simulation_clock;
    FTestEntityRegistry* entity_registry{nullptr};
    FSpatialQueryManager const* spatial_query_manager{nullptr};
    ml::test_lasers::Simulation* laser_simulation{nullptr};
    EntityData entities{};
    EntityDeathInfo entity_death_info;
    RegistryEntityData entity_update_data;
    int32 target_refresh_next_offset{0};

    TArray<int32> scratch_int_buffer;
    FVectors3f line_of_sight_start_locations;
    FVectors3f line_of_sight_end_locations;
    TArray<FRegistryEntityHandle> line_of_sight_hit_entity_handles;
    ml::test_lasers::SpawnRequests new_lasers;
    TArray<int32> local_indices_to_remove;
    TArray<int32> presentation_indices_to_remove;
    TArray<FVector3f> presentation_death_locations;
};
} // namespace ml::test_static_turrets
