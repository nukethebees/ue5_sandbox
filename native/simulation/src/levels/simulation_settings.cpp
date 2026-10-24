#include <ioj/sim/levels/simulation_settings.h>

#include <ioj/sim/collision/collision_grid.h>
#include <ioj/sim/levels/level_sim_init_data.h>

#include <format>
#include <limits>
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
    auto tick_period = [&](double const ticks,
                           double const maximum,
                           std::string_view const path,
                           double const minimum = 0) {
        auto const period{std::ceil(ticks)};
        check(std::isfinite(period) && period >= minimum && period <= maximum,
              path,
              "Tick period is outside the countdown range");
    };
    auto frequency = [&](double const value,
                         std::string_view const path,
                         double const maximum = std::numeric_limits<std::int16_t>::max()) {
        positive(value, path);
        if (std::isfinite(value) && value > 0) {
            tick_period(settings.clock.tick_rate.get_hz() / value, maximum, path, 1);
        }
    };
    auto cooldown = [&](double const value, std::string_view const path) {
        nonnegative(value, path);
        if (std::isfinite(value) && value >= 0) {
            tick_period(value * settings.clock.tick_rate.get_hz(),
                        std::numeric_limits<std::int16_t>::max(),
                        path);
        }
    };

    positive(settings.player.health, "player-ship.health");
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
    frequency(fighters.awareness_scan_frequency,
              "fighters.awareness-scan-frequency",
              std::numeric_limits<std::int8_t>::max());
    frequency(fighters.attack_reposition_frequency, "fighters.attack-reposition-frequency");
    frequency(settings.turrets.target_refresh_frequency, "turrets.target-refresh-frequency");
    frequency(fighters.avoidance_clear_update_frequency,
              "fighters.avoidance-clear-update-frequency");
    frequency(fighters.avoidance_update_frequency, "fighters.avoidance-update-frequency");
    frequency(fighters.avoidance_active_update_frequency,
              "fighters.avoidance-active-update-frequency");
    frequency(fighters.avoidance_immediate_update_frequency,
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
    cooldown(fighters.attack_retry_cooldown, "fighters.attack-retry-cooldown");

    auto laser = [&](std::string_view const section,
                     LaserWeaponSimConfig const& config,
                     bool const tick_countdown = true) {
        positive(config.projectile_speed, std::format("{}.laser.projectile-speed", section));
        positive(config.max_distance, std::format("{}.laser.max-distance", section));
        auto const path{std::format("{}.laser.fire-cooldown", section)};
        if (tick_countdown) {
            cooldown(config.fire_cooldown, path);
        } else {
            nonnegative(config.fire_cooldown, path);
        }
    };
    laser("player-ship", settings.player.laser, false);
    laser("fighters", settings.fighters.laser);
    laser("turrets", settings.turrets.laser);
    laser("tube-spinners", settings.spinners.laser);

    auto const dimensions{collision::calculate_grid_dimensions(
        settings.overlap_response.level_size, settings.overlap_response.cell_size)};
    check(collision::is_configured({dimensions, settings.overlap_response.cell_size}),
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
    target.collision.overlap_response = settings.overlap_response;

    auto const fighter_fire_point{target.fighters.fire_point_distance};
    target.fighters = settings.fighters;
    target.fighters.fire_point_distance = fighter_fire_point;

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
