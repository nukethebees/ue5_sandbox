#pragma once

#include <SpaceGame/simulation/LevelSimulationConfig.h>

#include <SpaceGame/combat/lasers/TestLasersSoA.h>
#include <SpaceGame/entities/DirectDamageEvents.h>
#include <SpaceGame/simulation/LineTraces.h>
#include <SpaceGame/simulation/SimulationClockInterface.h>
#include <SpaceGame/simulation/TraceHits.h>

#include <CoreMinimal.h>

class ATestBatchOrchestrator;
class FLaserPresentationIndexingTest;
struct FLevelSimulation;
struct FLaserPresentation;
struct FTestEntityRegistry;

namespace ml {
struct FSpatialQueryManager;
}

namespace ml::test_lasers {
class PhaseInterface;

struct ThreadLocalCollisionData {
    FLineTraces traces;
    FTraceHits trace_hits;
    DirectDamageEvents damage_events;
    TArray<int32> to_remove;
    HitDetails hit_details;
};

struct SPACEGAME_API Simulation {
    using SpawnRequests = ml::test_lasers::SpawnRequests;
    using Entities = ml::test_lasers::Entities;
    using HitDetails = ml::test_lasers::HitDetails;

    void bind_simulation_clock(FSimulationClock const& clock) noexcept;
    void set_entity_registry(FTestEntityRegistry& new_entity_registry) noexcept;
    void set_spatial_query_manager(FSpatialQueryManager& new_query_manager) noexcept;

    auto get_num_instances() const noexcept -> int32;
    auto get_entity_registry() const noexcept -> FTestEntityRegistry const* {
        return entity_registry;
    }
    auto get_number_spawned() const noexcept -> int32 { return number_spawned; }

    void queue_laser_spawns(SpawnRequests const& spawn_data);
    void validate_array_sizes() const;

    int32 n_preallocated_instances{5000};
    int32 collision_jobs{8};
  private:
    void clear_runtime_state();
    void begin_play();
    void begin_tick();
    void commit_spawns();
    void simulate(float dt);
    void end_tick();

    void preallocate_instances();
    void process_pending_spawns();

    void update_locations(float dt);
    void handle_collisions(float dt);
    static void check_collision_thread(int32 job_index,
                                       int32 updates_per_slice,
                                       float dt,
                                       ThreadLocalCollisionData& data,
                                       Simulation const& simulation);
    void merge_collision_data();

    void tick_lifetimes(float dt);
    void collect_old_instance_indices();
    void remove_instances(TConstArrayView<int32> indices);

    void clear_spawn_buffers();
    void clear_hit_buffers();
    void clear_presentation_events();

    friend class PhaseInterface;
    friend struct ::FLaserPresentation;
    friend class ::FLaserPresentationIndexingTest;

    FTestEntityRegistry* entity_registry{nullptr};
    FSpatialQueryManager* query_manager{nullptr};
    ml::test_batch_orchestrator::SimulationClockInterface simulation_clock;

    Entities entities;
    SpawnRequests pending_spawns;
    TArray<int32> to_remove;

    TArray<ThreadLocalCollisionData> thread_local_collision_data;
    DirectDamageEvents collision_damage_events;
    HitDetails hit_details;

    TArray<int32> presentation_indices_to_remove;
    TArray<float> presentation_custom_data_to_add;
    int32 presentation_spawn_count{0};

    int32 number_spawned{0};
};
} // namespace ml::test_lasers
