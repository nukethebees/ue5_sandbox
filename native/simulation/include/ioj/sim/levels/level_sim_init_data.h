#pragma once

#include <ioj/levels/level_metadata.h>
#include <ioj/settings/tick_settings.h>
#include <ioj/sim/capital_ships/capital_ship_sim_config.h>
#include <ioj/sim/collision/cell_coord.h>
#include <ioj/sim/collision/cell_coord_bounds.h>
#include <ioj/sim/collision/collision_init_data.h>
#include <ioj/sim/entities/team_list.h>
#include <ioj/sim/fighters/fighter_sim_config.h>
#include <ioj/sim/lasers/laser_sim_config.h>
#include <ioj/sim/levels/compiled_level_events.h>
#include <ioj/sim/missions/mission.h>
#include <ioj/sim/player/sim.h>
#include <ioj/sim/spinners/spinner_sim_config.h>
#include <ioj/sim/telemetry/recording_config.h>
#include <ioj/sim/telemetry/run_metadata.h>
#include <ioj/sim/turrets/turret_sim_config.h>

#include <cstddef>
#include <optional>

namespace ioj::sim {
struct LevelSimInitData {
    settings::TickSettings clock_settings{};

    LaserSimConfig lasers;
    CapitalShipSimConfig capital_ships;
    FighterSimConfig fighters;
    TurretSimConfig turrets;
    SpinnerSimConfig spinners;

    std::optional<player::PlayerSpawnData> player;
    CompiledLevelEvents level_events;
    Mission mission;
    levels::LevelMetadata level_metadata;

    CollisionInitData collision;

    std::size_t tick_memory_capacity_bytes{256 * 1024 * 1024};
    std::size_t phase_memory_capacity_bytes{256 * 1024 * 1024};

    telemetry::RunMetadata telemetry_metadata{};
    telemetry::RecordingConfig telemetry_recording{};
};

}
