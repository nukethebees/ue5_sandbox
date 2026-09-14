#pragma once
#include <cstdint>
#include <ioj/sim/system_read_views.h>
#include <optional>
#include <span>
#include <vector>

#include <ioj/sim/sim_config.h>

#include <ioj/sim/laser_frame_output.h>
#include <ioj/sim/laser_soa.h>
#include <ioj/sim/lasers/frame_scratch.h>
#include <ioj/sim/sim_clock.h>

#include <memory_resource>

namespace ioj::sim {
struct LevelSim;
struct EntityRegistry;
struct SpatialQueryManager;
}

namespace ioj::sim::lasers {
class PhaseInterface;

struct Sim {
    using SpawnRequests = ioj::sim::lasers::SpawnRequests;
    using Entities = ioj::sim::lasers::Entities;

    /* **************************************** */
    // Construction and access
    /* **************************************** */
    Sim(SimClock const& clock,
        EntityRegistry& entity_registry,
        SpatialQueryManager& query_manager,
        std::pmr::memory_resource& frame_memory_resource) noexcept;
    Sim(Sim const&) = delete;
    Sim(Sim&&) = delete;
    auto operator=(Sim const&) -> Sim& = delete;
    auto operator=(Sim&&) -> Sim& = delete;

    auto get_read_view() const -> LaserReadView {
        return {entities.get_const_view(),
                frame_output_.hits.get_const_view(),
                frame_output_.hit_ticks,
                frame_output_.hit_ordinals};
    }
    void reset_frame_output() { frame_output_.reset(); }
    auto get_num_instances() const noexcept -> std::int32_t;
    auto get_entity_registry() const noexcept -> EntityRegistry const& { return entity_registry; }
    auto get_number_spawned() const noexcept -> std::int32_t { return number_spawned; }

    /* **************************************** */
    // Spawning and configuration
    /* **************************************** */
    void queue_laser_spawns(ioj::sim::lasers::SpawnRequestsConstView spawn_data);
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
    void expire_instances(float dt);
    void update_locations(float dt);
    void handle_collisions(float dt);
    void remove_instances(std::span<std::int32_t const> indices);

    /* **************************************** */
    // Buffer cleanup
    /* **************************************** */
    void clear_spawn_buffers();
    // Dependencies and state
    /* **************************************** */
    friend class PhaseInterface;

    EntityRegistry& entity_registry;
    SpatialQueryManager& query_manager;
    std::pmr::memory_resource& frame_memory_resource;
    SimClock const& simulation_clock;

    Entities entities;
    SpawnRequests pending_spawns;

    ioj::sim::lasers::FrameOutput frame_output_;

    std::int32_t number_spawned{0};
};
} // namespace ioj::sim::lasers
