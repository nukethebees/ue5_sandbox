#include <ioj/sim/levels/simulation_settings.h>

#include <ioj/levels/level_definition.h>
#include <ioj/sim/collision/collision_grid.h>
#include <ioj/sim/levels/level_sim_init_data.h>

#include <format>
#include <utility>

namespace ioj::sim {
namespace simulation_settings {
// Keep the schema next to the native destination without a second configuration structure.
template <typename Apply>
void fields(SimulationSettings& target, Apply&& apply) {
    apply("tick.tick-rate", target.clock.tick_rate);
    apply("tick.time-scale", target.clock.time_scale);

    apply("player-ship.thrust-energy-max", target.player.thrust_energy_max);
    apply("player-ship.pitch-angle-max", target.player.pitch_angle_max);
    apply("player-ship.pitch-speed", target.player.pitch_speed);
    apply("player-ship.yaw-angle-max", target.player.yaw_angle_max);
    apply("player-ship.yaw-speed", target.player.yaw_speed);
    apply("player-ship.turn-bank-angle-max", target.player.turn_bank_angle_max);
    apply("player-ship.turn-bank-speed", target.player.turn_bank_speed);

    apply("laser-projectiles.n-preallocated-instances", target.lasers.n_preallocated_instances);
    apply("capital-ships.spawn-delay", target.capital_ships.spawn_delay);
    apply("capital-ships.max-health", target.capital_ships.max_health);

    apply("fighters.max-live-fighters", target.fighters.max_live_fighters);
    apply("fighters.fire-dot-product-threshold", target.fighters.fire_dot_product_threshold);
    apply("fighters.speed", target.fighters.speed);
    apply("fighters.turn-speed-unitless", target.fighters.turn_speed_unitless);
    apply("fighters.avoidance-clear-update-frequency",
          target.fighters.avoidance_clear_update_frequency);
    apply("fighters.avoidance-update-frequency", target.fighters.avoidance_update_frequency);
    apply("fighters.avoidance-active-update-frequency",
          target.fighters.avoidance_active_update_frequency);
    apply("fighters.avoidance-immediate-update-frequency",
          target.fighters.avoidance_immediate_update_frequency);
    apply("fighters.avoidance-lookahead-time", target.fighters.avoidance_lookahead_time);
    apply("fighters.avoidance-clearance-buffer", target.fighters.avoidance_clearance_buffer);
    apply("fighters.separation-radius", target.fighters.separation_radius);
    apply("fighters.separation-strength", target.fighters.separation_strength);
    apply("fighters.steering-memory-duration", target.fighters.steering_memory_duration);
    apply("fighters.dense-traffic-neighbour-threshold",
          target.fighters.dense_traffic_neighbour_threshold);
    apply("fighters.health", target.fighters.health);
    apply("fighters.attack-retry-cooldown", target.fighters.attack_retry_cooldown);
    apply("fighters.attack-engagement-threshold", target.fighters.attack_engagement_threshold);
    apply("fighters.attack-reposition-frequency", target.fighters.attack_reposition_frequency);
    apply("fighters.attack-distance-band.minimum-ratio",
          target.fighters.attack_distance_band.minimum_ratio);
    apply("fighters.attack-distance-band.desired-ratio",
          target.fighters.attack_distance_band.desired_ratio);
    apply("fighters.attack-distance-band.maximum-ratio",
          target.fighters.attack_distance_band.maximum_ratio);
    apply("fighters.arrival-distance", target.fighters.arrival_distance);
    apply("fighters.los-check-buffer", target.fighters.los_check_buffer);
    apply("fighters.awareness-radius", target.fighters.awareness_radius);
    apply("fighters.awareness-scan-frequency", target.fighters.awareness_scan_frequency);
    apply("fighters.minimum-opportunistic-intercept-deviation-dot-product",
          target.fighters.minimum_opportunistic_intercept_deviation_dot_product);

    apply("turrets.detection-radius", target.turrets.detection_radius);
    apply("turrets.target-refresh-frequency", target.turrets.target_refresh_frequency);
    apply("turrets.max-health", target.turrets.max_health);
    apply("tube-spinners.yaw-rotation-speed-degrees", target.spinners.yaw_rotation_speed_degrees);

    auto laser = [&](std::string_view const section, LaserWeaponSimConfig& config) {
        apply(std::format("{}.laser.damage", section), config.damage);
        apply(std::format("{}.laser.projectile-speed", section), config.projectile_speed);
        apply(std::format("{}.laser.max-distance", section), config.max_distance);
        apply(std::format("{}.laser.fire-cooldown", section), config.fire_cooldown);
    };
    laser("player-ship", target.player.laser);
    laser("fighters", target.fighters.laser);
    laser("turrets", target.turrets.laser);
    laser("tube-spinners", target.spinners.laser);

    apply("collision-grid.level-size", target.collision_grid_size);
    apply("collision-grid.cell-size", target.collision_cell_size);
    apply("collision-grid.damage-per-overlap-detection",
          target.overlap_response.damage_per_overlap_detection);

    apply("hud.player-status-update-period", target.hud.player_status_update_period);
    apply("hud.entity-count-update-period", target.hud.entity_count_update_period);
    apply("hud.mission-status-update-period", target.hud.mission_status_update_period);
}
}

auto apply_settings(settings::Settings const& settings, SimulationSettings& target)
    -> std::expected<void, levels::Diagnostics> {
    levels::Diagnostics errors;
    for (auto const& setting : settings) {
        bool found{};
        simulation_settings::fields(target, [&](std::string_view const path, auto& field) {
            if (setting.path == path) {
                found = true;
                if (!setting.assign(field)) {
                    errors.emplace_back(levels::DiagnosticCode::InvalidType,
                                        setting.path,
                                        "Setting has an incompatible value");
                }
            }
        });
        if (!found) {
            auto const section{std::string_view{setting.path}.substr(0, setting.path.find('.'))};
            if (section != "presentation") {
                errors.emplace_back(levels::DiagnosticCode::UnknownProperty,
                                    setting.path,
                                    "Unknown simulation setting");
            }
        }
    }
    if (!errors.empty()) {
        return std::unexpected{std::move(errors)};
    }
    return {};
}

auto apply_settings(levels::LevelDefinition const& level, SimulationSettings& target)
    -> std::expected<void, levels::Diagnostics> {
    auto applied{apply_settings(level.settings(), target)};
    if (!applied) {
        return applied;
    }

    if (auto const& grid{level.collision_grid()}; grid) {
        auto const vector = [](Vector3d const value) {
            return Vector3f{static_cast<float>(value.x()),
                            static_cast<float>(value.y()),
                            static_cast<float>(value.z())};
        };
        if (grid->level_size) {
            target.collision_grid_size = vector(*grid->level_size);
        }
        if (grid->cell_size) {
            target.collision_cell_size = vector(*grid->cell_size);
        }
    }
    return {};
}

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
        target.player->config = settings.player;
    }
}
}
