#pragma once

#include <ioj/sim/level_sim.h>
#include <utility>

namespace ioj::sim {

struct LevelSimTestAccess {
    static void queue_direct_damage_events(LevelSim& simulation,
                                           DirectDamageEventsConstView events) {
        simulation.entity_registry_.queue_direct_damage_events(events);
    }
    static void queue_laser_spawns(LevelSim& simulation, lasers::SpawnRequestsConstView requests) {
        simulation.lasers_simulation_.queue_laser_spawns(requests);
    }
    static void begin_telemetry_run(LevelSim& simulation, LevelTelemetryRunMetadata metadata) {
        simulation.level_telemetry_manager_.begin_run(std::move(metadata));
    }
    static auto complete_mission(LevelSim& simulation) -> bool {
        return simulation.mission_manager_.complete_mission();
    }
};

} // namespace ioj::sim
