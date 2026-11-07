#pragma once

#include <ioj/levels/diagnostics.h>
#include <ioj/settings/settings.h>
#include <ioj/settings/tick_settings.h>
#include <ioj/sim/capital_ships/capital_ship_sim_config.h>
#include <ioj/sim/collision/grid_geometry.h>
#include <ioj/sim/collision/overlap_response_config.h>
#include <ioj/sim/fighters/fighter_sim_config.h>
#include <ioj/sim/lasers/laser_sim_config.h>
#include <ioj/sim/levels/hud_settings.h>
#include <ioj/sim/player/fire_rate.h>
#include <ioj/sim/player/player_sim_config.h>
#include <ioj/sim/player/ship_laser_mode.h>
#include <ioj/sim/spinners/spinner_sim_config.h>
#include <ioj/sim/turrets/turret_sim_config.h>

#include <expected>
#include <optional>
#include <utility>

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

class PreparedSimulationSettings {
  public:
    [[nodiscard]] static auto prepare(SimulationSettings settings)
        -> std::expected<PreparedSimulationSettings, levels::Diagnostics>;

    [[nodiscard]] auto values() const noexcept -> SimulationSettings const& { return values_; }
    [[nodiscard]] auto grid_geometry() const noexcept -> collision::GridGeometry {
        return grid_geometry_;
    }

  private:
    PreparedSimulationSettings(SimulationSettings values, collision::GridGeometry grid_geometry)
        : values_{std::move(values)}
        , grid_geometry_{grid_geometry} {}

    SimulationSettings values_;
    collision::GridGeometry grid_geometry_;
};

[[nodiscard]] auto
    resolve_simulation_settings(settings::Settings const& source,
                                std::optional<settings::TickSettings> clock_override = std::nullopt)
        -> std::expected<PreparedSimulationSettings, levels::Diagnostics>;

// Retain the destination's asset-derived geometry and spawn data.
void apply_settings(PreparedSimulationSettings const& settings, LevelSimInitData& target);
void apply_settings(PreparedSimulationSettings const& settings, player::PlayerSpawnData& target);
}
