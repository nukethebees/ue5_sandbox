#pragma once

#include <ioj/column_math.h>
#include <ioj/frame_memory_resource.h>
#include <ioj/sim/levels/level_sim.h>
#include <ioj/vector_operations.h>

#include <algorithm>
#include <utility>

namespace ioj::sim {

struct LevelSimTestAccess {
    static auto spatial_queries(LevelSim& simulation) -> SpatialQueryManager& {
        return simulation.collision_pipeline_.queries();
    }

    static auto player_simulation(LevelSim& simulation) -> player::Sim& {
        return simulation.player_ship_simulation_.value();
    }
    static void queue_fighter_spawns(LevelSim& simulation, FighterSpawnQueue::ConstView spawns) {
        simulation.fighters_simulation_.queue_spawns(spawns);
    }
    static void queue_direct_damage_events(LevelSim& simulation,
                                           DirectDamageEvents::ConstView events) {
        simulation.combat_events_.queue_damage(events);
    }
    static void queue_laser_spawns(LevelSim& simulation,
                                   lasers::LaserSpawnRequests::ConstView requests) {
        auto const destination{simulation.lasers_simulation_.append_spawn_requests(requests.num())};
        copy_vectors(destination.view_locations(), requests.view_locations());
        auto const rotations{destination.view_rotations().resolve()};
        auto const source_rotations{requests.view_rotations().resolve()};
        std::ranges::copy(source_rotations.pitches(), rotations.pitches().begin());
        std::ranges::copy(source_rotations.yaws(), rotations.yaws().begin());
        std::ranges::copy(source_rotations.rolls(), rotations.rolls().begin());
        copy_vectors(destination.view_base_velocities(), requests.view_base_velocities());
        std::ranges::copy(requests.damages(), destination.damages().begin());
        std::ranges::copy(requests.speeds(), destination.speeds().begin());
        std::ranges::copy(requests.max_distances(), destination.max_distances().begin());
        std::ranges::copy(requests.instigator_ids(), destination.instigator_ids().begin());
        std::ranges::copy(requests.sources(), destination.sources().begin());
    }
};

} // namespace ioj::sim
