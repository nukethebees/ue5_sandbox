#include <ioj/sim/levels/simulation_settings.h>

#include <ioj/sim/collision/collision_grid.h>
#include <ioj/sim/levels/level_sim_init_data.h>

#include <limits>
#include <utility>

namespace ioj::sim {
auto resolve_simulation_settings(settings::Settings const& source,
                                 std::optional<settings::TickSettings> const clock_override)
    -> std::expected<SimulationSettings, levels::Diagnostics> {
    SimulationSettings result{
        .clock = source.tick,
        .player = source.player_ship,
        .lasers = source.laser_projectiles,
        .capital_ships = source.capital_ships,
        .fighters = source.fighters,
        .turrets = source.turrets,
        .spinners = source.tube_spinners,
        .overlap_response = source.collision_grid,
        .hud = source.hud,
    };
    if (auto valid{validate_settings(result, clock_override)}; !valid) {
        return std::unexpected{std::move(valid.error())};
    }
    result.clock = clock_override.value_or(result.clock);
    return result;
}

auto validate_settings(SimulationSettings const& settings,
                       std::optional<settings::TickSettings> const clock_override)
    -> std::expected<void, levels::Diagnostics> {
    levels::Diagnostics errors;
    auto check =
        [&](bool const valid, std::string_view const path, std::string_view const message) {
            if (!valid) {
                errors.emplace_back(
                    levels::DiagnosticCode::InvalidNumber, std::string{path}, std::string{message});
            }
        };
    auto tick_period =
        [&](double const ticks, std::string_view const path, double const minimum = 0) {
            auto const period{std::ceil(ticks)};
            check(std::isfinite(period) && period >= minimum &&
                      period <= std::numeric_limits<std::int16_t>::max(),
                  path,
                  "Tick period is outside the countdown range");
        };
    auto validate_countdowns = [&](double const tick_rate) {
        auto frequency = [&](PositiveFinite<float> const value, std::string_view const path) {
            tick_period(tick_rate / value, path, 1);
        };
        auto cooldown = [&](NonNegativeFinite<float> const value, std::string_view const path) {
            tick_period(value * tick_rate, path);
        };

        auto const& fighters{settings.fighters};
        frequency(settings.turrets.target_refresh_frequency, "turrets.target-refresh-frequency");
        frequency(fighters.avoidance_clear_update_frequency,
                  "fighters.avoidance-clear-update-frequency");
        frequency(fighters.avoidance_update_frequency, "fighters.avoidance-update-frequency");
        frequency(fighters.avoidance_active_update_frequency,
                  "fighters.avoidance-active-update-frequency");
        frequency(fighters.avoidance_immediate_update_frequency,
                  "fighters.avoidance-immediate-update-frequency");
        cooldown(fighters.attack_retry_cooldown, "fighters.attack-retry-cooldown");
        cooldown(fighters.laser.fire_cooldown, "fighters.laser.fire-cooldown");
        cooldown(settings.turrets.laser.fire_cooldown, "turrets.laser.fire-cooldown");
        cooldown(settings.spinners.laser.fire_cooldown, "tube-spinners.laser.fire-cooldown");
    };
    validate_countdowns(settings.clock.tick_rate.hz());

    auto const& fighters{settings.fighters};
    check(fighters.avoidance_clear_update_frequency <= fighters.avoidance_update_frequency &&
              fighters.avoidance_update_frequency <= fighters.avoidance_active_update_frequency &&
              fighters.avoidance_active_update_frequency <=
                  fighters.avoidance_immediate_update_frequency,
          "fighters.avoidance-update-frequency",
          "Avoidance frequencies must increase with risk");

    auto const dimensions{collision::calculate_grid_dimensions(
        settings.overlap_response.level_size, settings.overlap_response.cell_size)};
    auto const geometry{
        collision::GridGeometry::try_make(dimensions, settings.overlap_response.cell_size)};
    check(geometry.has_value(),
          "collision-grid",
          "Expected positive grid dimensions with a representable cell count and finite extents");

    if (errors.empty() && clock_override && clock_override->tick_rate != settings.clock.tick_rate) {
        validate_countdowns(clock_override->tick_rate.hz());
    }

    if (!errors.empty()) {
        return std::unexpected{std::move(errors)};
    }
    return {};
}

void apply_settings(SimulationSettings const& settings, LevelSimInitData& target) {
    target.clock_settings = settings.clock;
    target.lasers = settings.lasers;
    target.collision.overlap_response = settings.overlap_response;

    auto const fighter_fire_point{target.fighters.fire_point_distance};
    target.fighters = settings.fighters;
    target.fighters.fire_point_distance = fighter_fire_point;

    auto spawn_transforms{std::move(target.capital_ships.fighter_spawn_slots_relative_transforms)};
    target.capital_ships = settings.capital_ships;
    target.capital_ships.fighter_spawn_slots_relative_transforms = std::move(spawn_transforms);

    auto const turret_fire_point{target.turrets.fire_point_offset};
    target.turrets = settings.turrets;
    target.turrets.fire_point_offset = turret_fire_point;

    auto spinner_fire_points{std::move(target.spinners.fire_point_offsets)};
    target.spinners = settings.spinners;
    target.spinners.fire_point_offsets = std::move(spinner_fire_points);

    target.collision.grid_geometry = {
        collision::calculate_grid_dimensions(settings.overlap_response.level_size,
                                             settings.overlap_response.cell_size),
        settings.overlap_response.cell_size};
    if (target.player) {
        apply_settings(settings, *target.player);
    }
}

void apply_settings(SimulationSettings const& settings, player::PlayerSpawnData& target) {
    target.config = settings.player;
    target.health = ShipHealth{settings.player.health};
    target.laser_mode = settings.player.laser_mode;
    target.laser_fire_rate = settings.player.laser_fire_rate;
}
}
