#pragma once
#include <ioj/sim/laser_frame_output.h>
#include <ioj/sim/laser_soa.h>
#include <ioj/sim/lasers/frame_scratch.h>
#include <ioj/sim/sim_clock.h>
#include <ioj/sim/sim_config.h>
#include <ioj/sim/system_read_views.h>

#include <sandbox/core/frame_memory_resource.h>

#include <cstdint>
#include <span>
#include <vector>

namespace ioj::sim {
class CombatEvents;
struct LevelSim;
struct SpatialQueryManager;
}

namespace ioj::sim::lasers {
class PhaseInterface;

struct Sim {
    using SingleAllocationLaserSpawnRequests = lasers::SingleAllocationLaserSpawnRequests;
    using SingleAllocationLaserEntities = lasers::SingleAllocationLaserEntities;
    using SpawnRequestStorage = SingleAllocationLaserSpawnRequests;
    using EntityStorage = SingleAllocationLaserEntities;

    /* **************************************** */
    // Construction and access
    /* **************************************** */
    Sim(SimClock const& clock,
        CombatEvents& combat_events,
        SpatialQueryManager& query_manager) noexcept;
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
    auto get_number_spawned() const noexcept -> std::int32_t { return number_spawned; }

    /* **************************************** */
    // Spawning and configuration
    /* **************************************** */
    void set_config(LaserSimConfig const& new_config) noexcept;
    template <typename Source>
        requires SingleAllocationLaserSpawnRequests::accepts_source<Source>
    void queue_laser_spawns(Source const& spawn_data) {
        pending_spawns.append_from(spawn_data);
    }
  private:
    /* **************************************** */
    // Tick phases
    /* **************************************** */
    void begin_play();
    void commit_spawns();
    void cleanup_entities();
    void simulate(float dt, ml::FrameScratch& scratch);
    void finish_action();

    /* **************************************** */
    // Spawn preparation
    /* **************************************** */
    void preallocate_instances();
    void process_pending_spawns();

    /* **************************************** */
    // Movement and collision
    /* **************************************** */
    void expire_instances(float dt, ml::FrameScratch& scratch);
    void update_locations(float dt);
    void handle_collisions(float dt, ml::FrameScratch& scratch);
    void remove_instances(std::span<std::int32_t const> indices);

    /* **************************************** */
    // Buffer cleanup
    /* **************************************** */
    void clear_spawn_buffers();
    // Dependencies and state
    /* **************************************** */
    friend class PhaseInterface;

    CombatEvents& combat_events;
    SpatialQueryManager& query_manager;
    SimClock const& simulation_clock;
    LaserSimConfig config{};

    EntityStorage entities;
    SpawnRequestStorage pending_spawns;

    FrameOutput frame_output_;

    std::int32_t number_spawned{0};
    std::vector<std::int32_t> pending_removals_;
};
} // namespace ioj::sim::lasers
