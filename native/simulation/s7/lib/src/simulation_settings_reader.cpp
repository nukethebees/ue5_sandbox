#include <ioj/s7/simulation_settings_reader.h>

#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/levels/level_definition.h>

#include <format>
#include <utility>

namespace ioj::s7 {
namespace simulation_settings {
using sim::LaserWeaponSimConfig;
using sim::ShipFireRate;
using sim::ShipLaserMode;
using sim::SimulationSettings;
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
    apply("player-ship.health", target.player_initial.health);
    apply("player-ship.laser-mode", target.player_initial.laser_mode);
    apply("player-ship.laser-fire-rate", target.player_initial.laser_fire_rate);

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

template <typename T>
auto assign(settings::Setting const& setting, T& target) -> bool {
    return setting.assign(target);
}

auto assign(settings::Setting const& setting, ShipLaserMode& target) -> bool {
    auto const* name{std::get_if<std::string>(&setting.value)};
    if (!name) {
        return false;
    }
    if (*name == "single") {
        target = ShipLaserMode::Single;
    } else if (*name == "double") {
        target = ShipLaserMode::Double;
    } else if (*name == "hyper") {
        target = ShipLaserMode::Hyper;
    } else {
        return false;
    }
    return true;
}

auto assign(settings::Setting const& setting, ShipFireRate& target) -> bool {
    auto const* name{std::get_if<std::string>(&setting.value)};
    if (!name) {
        return false;
    }
    if (*name == "single") {
        target = ShipFireRate::Single;
    } else if (*name == "burst3") {
        target = ShipFireRate::Burst3;
    } else if (*name == "full-auto") {
        target = ShipFireRate::FullAuto;
    } else {
        return false;
    }
    return true;
}

auto apply_settings(settings::Settings const& settings, SimulationSettings& target)
    -> std::expected<void, levels::Diagnostics> {
    levels::Diagnostics errors;
    for (auto const& setting : settings) {
        bool found{};
        simulation_settings::fields(target, [&](std::string_view const path, auto& field) {
            if (setting.path == path) {
                found = true;
                if (!simulation_settings::assign(setting, field)) {
                    errors.emplace_back(levels::DiagnosticCode::InvalidType,
                                        setting.path,
                                        "Setting has an incompatible value");
                }
            }
        });
        if (!found) {
            auto const section{std::string_view{setting.path}.substr(0, setting.path.find('.'))};
            if (section != "presentation" && section != "starfield") {
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

}

auto resolve_simulation_settings(settings::Settings const& source,
                                 levels::LevelDefinition const* const level)
    -> SimulationSettingsReadResult {
    sim::SimulationSettings result{};
    auto applied{simulation_settings::apply_settings(source, result)};
    if (applied && level) {
        applied = simulation_settings::apply_settings(*level, result);
    }
    if (applied) {
        applied = sim::validate_settings(result);
    }
    if (!applied) {
        return std::unexpected{std::move(applied.error())};
    }
    return result;
}

auto read_simulation_settings(std::filesystem::path const& path,
                              std::filesystem::path const& library_root,
                              levels::LevelDefinition const* const level)
    -> SimulationSettingsReadResult {
    auto source{levels::authoring::DefinitionReader{library_root}.read_settings_file(path)};
    if (!source) {
        return std::unexpected{std::move(source.error())};
    }
    return resolve_simulation_settings(*source, level);
}
}
