#pragma once
#include <cstdint>
#include <optional>
#include <sandbox/simulation/simulation/SystemReadViews.h>
#include <span>
#include <vector>

#include <sandbox/simulation/simulation/LevelSimulationConfig.h>

#include <sandbox/simulation/combat/lasers/TestLasersFrameScratch.h>
#include <sandbox/simulation/laser_frame_output.h>
#include <sandbox/simulation/laser_soa.h>
#include <sandbox/simulation/simulation/SimulationClock.h>

#include <memory_resource>

struct FLevelSimulation;
struct FTestEntityRegistry;

namespace ml {
struct FSpatialQueryManager;
}

namespace ml::test_lasers {
class PhaseInterface;

struct Simulation {
    using SpawnRequests = ml::simulation::lasers::SpawnRequests;
    using Entities = ml::simulation::lasers::Entities;

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
        return {entities.get_const_view(),
                frame_output_.hits.get_const_view(),
                frame_output_.hit_ticks,
                frame_output_.hit_ordinals};
    }
    void reset_frame_output() { frame_output_.reset(); }
    auto get_num_instances() const noexcept -> std::int32_t;
    auto get_entity_registry() const noexcept -> FTestEntityRegistry const& {
        return entity_registry;
    }
    auto get_number_spawned() const noexcept -> std::int32_t { return number_spawned; }

    /* **************************************** */
    // Spawning and configuration
    /* **************************************** */
    void queue_laser_spawns(ml::simulation::lasers::SpawnRequestsConstView spawn_data);
    void queue_laser_spawns(SpawnRequests const& spawn_data) {
        queue_laser_spawns(spawn_data.get_const_view());
    }
    void validate_array_sizes() const;

    std::int32_t n_preallocated_instances{5000};
    std::int32_t collision_jobs{8};
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
    void handle_collisions(float dt);

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
    FSimulationClock const& simulation_clock;

    Entities entities;
    SpawnRequests pending_spawns;

    ml::simulation::lasers::FrameOutput frame_output_;

    std::int32_t number_spawned{0};
};
} // namespace ml::test_lasers
