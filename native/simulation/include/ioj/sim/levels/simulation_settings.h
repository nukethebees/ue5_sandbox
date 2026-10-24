#pragma once

#include <ioj/levels/diagnostics.h>
#include <ioj/settings/tick_settings.h>
#include <ioj/sim/capital_ships/capital_ship_sim_config.h>
#include <ioj/sim/collision/overlap_response_config.h>
#include <ioj/sim/fighters/fighter_sim_config.h>
#include <ioj/sim/lasers/laser_sim_config.h>
#include <ioj/sim/levels/hud_settings.h>
#include <ioj/sim/player/fire_rate.h>
#include <ioj/sim/player/player_sim_config.h>
#include <ioj/sim/player/ship_laser_mode.h>
#include <ioj/sim/spinners/spinner_sim_config.h>
#include <ioj/sim/turrets/turret_sim_config.h>

namespace ioj::sim {
struct LevelSimInitData;
namespace player {
struct PlayerSpawnData;
}

struct SimulationSettings {
    settings::TickSettings clock{};
    PlayerSimConfig player{};
    LaserSimConfig lasers{};
    CapitalShipSimConfig capital_ships{};
    FighterSimConfig fighters{};
    TurretSimConfig turrets{};
    SpinnerSimConfig spinners{};
    OverlapResponseConfig overlap_response{};
    HudSettings hud{};
};

// Validate the complete result after all default and level overrides have been applied.
[[nodiscard]] auto validate_settings(SimulationSettings const& settings)
    -> std::expected<void, levels::Diagnostics>;

// Require validated settings. Retain the destination's asset-derived geometry and spawn data.
void apply_settings(SimulationSettings const& settings, LevelSimInitData& target);
void apply_settings(SimulationSettings const& settings, player::PlayerSpawnData& target);
}
