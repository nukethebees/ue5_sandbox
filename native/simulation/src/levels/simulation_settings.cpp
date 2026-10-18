#include <ioj/sim/levels/simulation_settings.h>

#include <ioj/sim/collision/collision_grid.h>
#include <ioj/sim/levels/level_sim_init_data.h>

#include <format>
#include <utility>

namespace ioj::sim {
auto validate_settings(SimulationSettings const& settings)
    -> std::expected<void, levels::Diagnostics> {
    levels::Diagnostics errors;
    auto check =
        [&](bool const valid, std::string_view const path, std::string_view const message) {
            if (!valid) {
                errors.emplace_back(
                    levels::DiagnosticCode::InvalidNumber, std::string{path}, std::string{message});
            }
        };
    auto positive = [&](double const value, std::string_view const path) {
        check(std::isfinite(value) && value > 0, path, "Expected a finite positive value");
    };
    auto nonnegative = [&](double const value, std::string_view const path) {
        check(std::isfinite(value) && value >= 0, path, "Expected a finite nonnegative value");
    };

    positive(settings.clock.tick_rate, "tick.tick-rate");
    positive(settings.clock.time_scale, "tick.time-scale");
    positive(settings.player.thrust_energy_max, "player-ship.thrust-energy-max");
    positive(settings.player_initial.health, "player-ship.health");
    nonnegative(settings.lasers.n_preallocated_instances,
                "laser-projectiles.n-preallocated-instances");
    nonnegative(settings.fighters.max_live_fighters, "fighters.max-live-fighters");
    positive(settings.capital_ships.max_health, "capital-ships.max-health");
    positive(settings.fighters.health, "fighters.health");
    positive(settings.turrets.max_health, "turrets.max-health");

    auto const& fighters{settings.fighters};
    check(std::isfinite(fighters.fire_dot_product_threshold) &&
              fighters.fire_dot_product_threshold >= -1 && fighters.fire_dot_product_threshold <= 1,
          "fighters.fire-dot-product-threshold",
          "Expected a value in [-1, 1]");
    positive(fighters.avoidance_clear_update_frequency,
             "fighters.avoidance-clear-update-frequency");
    positive(fighters.avoidance_update_frequency, "fighters.avoidance-update-frequency");
    positive(fighters.avoidance_active_update_frequency,
             "fighters.avoidance-active-update-frequency");
    positive(fighters.avoidance_immediate_update_frequency,
             "fighters.avoidance-immediate-update-frequency");
    check(fighters.avoidance_clear_update_frequency <= fighters.avoidance_update_frequency &&
              fighters.avoidance_update_frequency <= fighters.avoidance_active_update_frequency &&
              fighters.avoidance_active_update_frequency <=
                  fighters.avoidance_immediate_update_frequency,
          "fighters.avoidance-update-frequency",
          "Avoidance frequencies must increase with risk");
    nonnegative(fighters.avoidance_lookahead_time, "fighters.avoidance-lookahead-time");
    nonnegative(fighters.avoidance_clearance_buffer, "fighters.avoidance-clearance-buffer");
    positive(fighters.separation_radius, "fighters.separation-radius");
    nonnegative(fighters.separation_strength, "fighters.separation-strength");
    nonnegative(fighters.steering_memory_duration, "fighters.steering-memory-duration");
    check(fighters.dense_traffic_neighbour_threshold >= 2,
          "fighters.dense-traffic-neighbour-threshold",
          "Expected at least two neighbours");
    check(fighters.attack_distance_band.values_are_valid(),
          "fighters.attack-distance-band",
          "Invalid attack distance band");

    auto laser = [&](std::string_view const section, LaserWeaponSimConfig const& config) {
        positive(config.projectile_speed, std::format("{}.laser.projectile-speed", section));
        positive(config.max_distance, std::format("{}.laser.max-distance", section));
        nonnegative(config.fire_cooldown, std::format("{}.laser.fire-cooldown", section));
    };
    laser("player-ship", settings.player.laser);
    laser("fighters", settings.fighters.laser);
    laser("turrets", settings.turrets.laser);
    laser("tube-spinners", settings.spinners.laser);

    auto const dimensions{collision::calculate_grid_dimensions(settings.collision_grid_size,
                                                               settings.collision_cell_size)};
    check(collision::is_configured({dimensions, settings.collision_cell_size}),
          "collision-grid",
          "Expected positive grid dimensions with a representable cell count");
    nonnegative(settings.hud.player_status_update_period, "hud.player-status-update-period");
    nonnegative(settings.hud.entity_count_update_period, "hud.entity-count-update-period");
    nonnegative(settings.hud.mission_status_update_period, "hud.mission-status-update-period");

    if (!errors.empty()) {
        return std::unexpected{std::move(errors)};
    }
    return {};
}

void apply_settings(SimulationSettings const& settings, LevelSimInitData& target) {
    target.clock_settings = settings.clock;
    target.lasers = settings.lasers;
    target.overlap_response = settings.overlap_response;
    target.fighters = settings.fighters;

    auto spawn_transforms{std::move(target.capital_ships.fighter_spawn_slots_relative_transforms)};
    auto const spawn_slots{target.capital_ships.fighter_spawn_slots};
    target.capital_ships = settings.capital_ships;
    target.capital_ships.fighter_spawn_slots = spawn_slots;
    target.capital_ships.fighter_spawn_slots_relative_transforms = std::move(spawn_transforms);

    auto const turret_fire_point{target.turrets.fire_point_offset};
    target.turrets = settings.turrets;
    target.turrets.fire_point_offset = turret_fire_point;

    auto spinner_fire_points{std::move(target.spinners.fire_point_offsets)};
    target.spinners = settings.spinners;
    target.spinners.fire_point_offsets = std::move(spinner_fire_points);

    target.grid_geometry = {collision::calculate_grid_dimensions(settings.collision_grid_size,
                                                                 settings.collision_cell_size),
                            settings.collision_cell_size};
    if (target.player) {
        apply_settings(settings, *target.player);
    }
}

void apply_settings(SimulationSettings const& settings, player::PlayerSpawnData& target) {
    target.config = settings.player;
    target.health = ShipHealth{settings.player_initial.health};
    target.laser_mode = settings.player_initial.laser_mode;
    target.laser_fire_rate = settings.player_initial.laser_fire_rate;
}
}
