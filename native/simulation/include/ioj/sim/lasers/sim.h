#pragma once
#include <ioj/frame_memory_resource.h>
#include <ioj/sim/lasers/laser_entities.h>
#include <ioj/sim/lasers/laser_frame_output.h>
#include <ioj/sim/lasers/laser_sim_config.h>
#include <ioj/sim/lasers/laser_spawn_requests.h>
#include <ioj/sim/lasers/phase_inputs.h>

#include <cstdint>
#include <span>
#include <vector>

namespace ioj::sim {
struct LevelSim;
}

namespace ioj::sim::lasers {
class PhaseInterface;

struct Sim {
    using LaserSpawnRequests = lasers::LaserSpawnRequests;
    using LaserEntities = lasers::LaserEntities;
    using SpawnRequestStorage = LaserSpawnRequests;
    using EntityStorage = LaserEntities;

    /* **************************************** */
    // Construction and access
    /* **************************************** */
    Sim() = default;
    Sim(Sim const&) = delete;
    Sim(Sim&&) = delete;
    auto operator=(Sim const&) -> Sim& = delete;
    auto operator=(Sim&&) -> Sim& = delete;

    auto get_entities() const -> EntityStorage::ConstView { return entities.const_view(); }
    auto hits() const -> LaserHitDetails::ConstView { return frame_output_.hits.const_view(); }
    auto hit_ticks() const -> std::span<SimTick const> { return frame_output_.hit_ticks; }
    auto hit_ordinals() const -> std::span<std::int32_t const> {
        return frame_output_.hit_ordinals;
    }
    void reset_frame_output() { frame_output_.reset(); }
    auto num_instances() const noexcept -> std::uint32_t;
    auto get_number_spawned() const noexcept -> std::uint32_t { return number_spawned; }

    /* **************************************** */
    // Spawning and configuration
    /* **************************************** */
    // Fill every appended column before another append or commit. Serialize appends and
    // discard the returned view and its spans before either operation can invalidate them.
    auto append_spawn_requests(std::uint32_t count) -> LaserSpawnRequests::View;
  private:
    /* **************************************** */
    // Tick phases
    /* **************************************** */
    void initialise(LaserSimConfig const& new_config);
    void commit_spawns(PreparationInputs const inputs);
    void cleanup_entities(ResolutionCommitInputs const inputs);
    void simulate(ActionInputs const inputs);
    void finish_action();

    /* **************************************** */
    // Spawn preparation
    /* **************************************** */
    void preallocate_instances();
    void process_pending_spawns(PreparationInputs const inputs);

    /* **************************************** */
    // Movement and collision
    /* **************************************** */
    void expire_instances(float dt, std::pmr::vector<LaserIndex>& pending_removals);
    void update_locations(float dt);
    void handle_collisions(ActionInputs const inputs);
    void remove_instances(std::span<LaserIndex const> indices);

    /* **************************************** */
    // Buffer cleanup
    /* **************************************** */
    void clear_spawn_buffers();

    friend class PhaseInterface;

    LaserSimConfig config{};

    EntityStorage entities;
    SpawnRequestStorage pending_spawns;

    FrameOutput frame_output_;

    std::uint32_t number_spawned{0};
};
} // namespace ioj::sim::lasers
