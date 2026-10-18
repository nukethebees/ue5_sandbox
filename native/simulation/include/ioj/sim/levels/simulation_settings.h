#pragma once

#include <ioj/levels/diagnostics.h>
#include <ioj/settings/settings.h>
#include <ioj/sim/capital_ships/capital_ship_sim_config.h>
#include <ioj/sim/collision/overlap_response_config.h>
#include <ioj/sim/fighters/fighter_sim_config.h>
#include <ioj/sim/fixed_tick_loop.h>
#include <ioj/sim/lasers/laser_sim_config.h>
#include <ioj/sim/player/fire_rate.h>
#include <ioj/sim/player/player_sim_config.h>
#include <ioj/sim/player/ship_laser_mode.h>
#include <ioj/sim/spinners/spinner_sim_config.h>
#include <ioj/sim/turrets/turret_sim_config.h>

namespace ioj::levels {
class LevelDefinition;
}

namespace ioj::sim {
struct LevelSimInitData;
namespace player {
struct PlayerSpawnData;
}

struct HudSettings {
    float player_status_update_period{0.25f};
    float entity_count_update_period{0.25f};
    float mission_status_update_period{0.25f};
};

struct PlayerInitialSettings {
    Health health{1000};
    ShipLaserMode laser_mode{ShipLaserMode::Single};
    ShipFireRate laser_fire_rate{ShipFireRate::FullAuto};
};

struct SimulationSettings {
    FixedTickLoop clock{};
    PlayerSimConfig player{};
    PlayerInitialSettings player_initial{};
    LaserSimConfig lasers{};
    CapitalShipSimConfig capital_ships{};
    FighterSimConfig fighters{};
    TurretSimConfig turrets{};
    SpinnerSimConfig spinners{};
    OverlapResponseConfig overlap_response{};
    Vector3f collision_grid_size{2000000.f, 2000000.f, 100000.f};
    Vector3f collision_cell_size{5000.f, 5000.f, 20000.f};
    HudSettings hud{};
};

// Apply ordered overrides; leave presentation namespaces for their respective consumers.
[[nodiscard]] auto apply_settings(settings::Settings const& settings, SimulationSettings& target)
    -> std::expected<void, levels::Diagnostics>;

// Apply level tuning, then explicit authored collision dimensions.
[[nodiscard]] auto apply_settings(levels::LevelDefinition const& level, SimulationSettings& target)
    -> std::expected<void, levels::Diagnostics>;

// Validate the complete result after all default and level overrides have been applied.
[[nodiscard]] auto validate_settings(SimulationSettings const& settings)
    -> std::expected<void, levels::Diagnostics>;

// Require validated settings. Retain the destination's asset-derived geometry and spawn data.
void apply_settings(SimulationSettings const& settings, LevelSimInitData& target);
void apply_settings(SimulationSettings const& settings, player::PlayerSpawnData& target);
}
