#pragma once
#include <SpaceGameSimulation/simulation/SystemReadViews.h>

#include <SpaceGameSimulation/simulation/LevelSimulationConfig.h>

#include <SpaceGameSimulation/combat/lasers/TestLasersFrameScratch.h>
#include <SpaceGameSimulation/combat/lasers/TestLasersSoA.h>
#include <SpaceGameSimulation/simulation/SimulationClockInterface.h>

#include <CoreMinimal.h>

#include <memory_resource>

struct FLevelSimulation;
struct FTestEntityRegistry;

namespace ml {
struct FSpatialQueryManager;
}

namespace ml::test_lasers {
class PhaseInterface;

struct SPACEGAMESIMULATION_API Simulation {
    using SpawnRequests = ml::test_lasers::SpawnRequests;
    using Entities = ml::test_lasers::Entities;
    using HitDetails = ml::test_lasers::HitDetails;

    /* **************************************** */
    // Construction and access
    /* **************************************** */
    Simulation(FSimulationClock const& clock,
               FTestEntityRegistry& entity_registry,
               FSpatialQueryManager& query_manager,
               std::pmr::memory_resource& frame_memory_resource) noexcept;
    Simulation(Simulation const&) = delete;
    Simulation(Simulation&&) = delete;
    auto operator=(Simulation const&) -> Simulation& = delete;
    auto operator=(Simulation&&) -> Simulation& = delete;

    auto get_read_view() const -> FLaserReadView {
        return {entities.get_const_view(), frame_hits_.get_const_view(), hit_ticks_, hit_ordinals_};
    }
    void reset_frame_output() {
        frame_hits_.reset();
        hit_ticks_.Reset();
        hit_ordinals_.Reset();
    }
    auto get_num_instances() const noexcept -> int32;
    auto get_entity_registry() const noexcept -> FTestEntityRegistry const& {
        return entity_registry;
    }
    auto get_number_spawned() const noexcept -> int32 { return number_spawned; }

    /* **************************************** */
    // Spawning and configuration
    /* **************************************** */
    void queue_laser_spawns(SpawnRequestsConstView spawn_data);
    void queue_laser_spawns(SpawnRequests const& spawn_data) {
        queue_laser_spawns(spawn_data.get_const_view());
    }
    void validate_array_sizes() const;

    int32 n_preallocated_instances{5000};
    int32 collision_jobs{8};
  private:
    /* **************************************** */
    // Tick phases
    /* **************************************** */
    void begin_play();
    void begin_tick();
    void commit_spawns();
    void simulate(float dt);
    void end_tick();

    /* **************************************** */
    // Spawn preparation
    /* **************************************** */
    void preallocate_instances();
    void process_pending_spawns();

    /* **************************************** */
    // Movement and collision
    /* **************************************** */
    void update_locations(float dt);
    void handle_collisions(float dt);

    /* **************************************** */
    // Lifetime and removal
    /* **************************************** */
    void tick_lifetimes(float dt);
    void collect_old_instance_indices(TFrameArray<int32>& indices);
    void remove_instances(TConstArrayView<int32> indices);

    /* **************************************** */
    // Buffer cleanup
    /* **************************************** */
    void clear_spawn_buffers();
    // Dependencies and state
    /* **************************************** */
    friend class PhaseInterface;

    FTestEntityRegistry& entity_registry;
    FSpatialQueryManager& query_manager;
    std::pmr::memory_resource& frame_memory_resource;
    ml::test_batch_orchestrator::SimulationClockInterface simulation_clock;

    Entities entities;
    SpawnRequests pending_spawns;

    HitDetails frame_hits_;
    TArray<uint64> hit_ticks_;
    TArray<int32> hit_ordinals_;

    int32 number_spawned{0};
};
} // namespace ml::test_lasers
