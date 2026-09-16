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
    using SpawnRequests = lasers::SpawnRequests;
    using Entities = lasers::Entities;
    using SpawnRequestStorage = SingleAllocationLaserSpawnRequests;
    using EntityStorage = SingleAllocationLaserEntities;

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
        return {entities.get_const_view().columns(),
                frame_output_.hits.get_const_view().columns(),
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
    void set_config(LaserSimConfig const& new_config) noexcept;
    void queue_laser_spawns(SpawnRequestsConstView spawn_data);
    void queue_laser_spawns(SpawnRequests const& spawn_data) {
        queue_laser_spawns(spawn_data.get_const_view());
    }
    void validate_array_sizes() const;
  private:
    /* **************************************** */
    // Tick phases
    /* **************************************** */
    void begin_play();
    void commit_spawns();
    void cleanup_entities();
    void simulate(float dt);
    void finish_action();

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
    LaserSimConfig config{};

    EntityStorage entities;
    SpawnRequestStorage pending_spawns;

    FrameOutput frame_output_;

    std::int32_t number_spawned{0};
    std::vector<std::int32_t> pending_removals_;
};
} // namespace ioj::sim::lasers
