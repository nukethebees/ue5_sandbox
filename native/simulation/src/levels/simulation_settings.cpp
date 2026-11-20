#include <ioj/sim/levels/simulation_settings.h>

#include <ioj/sim/collision/collision_grid.h>
#include <ioj/sim/levels/level_sim_init_data.h>
#include <ioj/sim/sim_time.h>

#include <limits>
#include <utility>

namespace ioj::sim {
auto validate_settings(settings::Settings const& settings)
    -> std::expected<void, levels::Diagnostics> {
    levels::Diagnostics errors;
    auto check =
        [&](bool const valid, std::string_view const path, std::string_view const message) {
            if (!valid) {
                errors.emplace_back(
                    levels::DiagnosticCode::InvalidNumber, std::string{path}, std::string{message});
            }
        };
    auto const tick_rate{settings.tick.tick_rate};
    auto frequency = [&](PositiveFinite<float> value, std::string_view path) {
        auto const ticks{std::ceil(tick_rate.hz() / value)};
        check(ticks >= 1 && ticks <= std::numeric_limits<std::int16_t>::max(),
              path,
              "Tick period is outside the countdown range");
    };
    auto cooldown = [&](NonNegativeFinite<float> value, std::string_view path) {
        check(
            try_duration_to_tick_period<std::int16_t>(tick_rate, Duration::make(value)).has_value(),
            path,
            "Tick period is outside the countdown range");
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
    check(fighters.avoidance_clear_update_frequency <= fighters.avoidance_update_frequency &&
              fighters.avoidance_update_frequency <= fighters.avoidance_active_update_frequency &&
              fighters.avoidance_active_update_frequency <=
                  fighters.avoidance_immediate_update_frequency,
          "fighters.avoidance-update-frequency",
          "Avoidance frequencies must increase with risk");
    cooldown(fighters.attack_retry_cooldown, "fighters.attack-retry-cooldown");
    cooldown(fighters.laser.fire_cooldown, "fighters.laser.fire-cooldown");
    cooldown(settings.turrets.laser.fire_cooldown, "turrets.laser.fire-cooldown");
    cooldown(settings.tube_spinners.laser.fire_cooldown, "tube-spinners.laser.fire-cooldown");

    auto const dimensions{collision::calculate_grid_dimensions(settings.collision_grid.level_size,
                                                               settings.collision_grid.cell_size)};
    check(collision::is_configured({dimensions, settings.collision_grid.cell_size}),
          "collision-grid",
          "Expected positive grid dimensions with a representable cell count");

    if (!errors.empty()) {
        return std::unexpected{std::move(errors)};
    }
    return {};
}

namespace {
auto transform(settings::AttachmentTransform const& value) -> Transform3d {
    return {Quaternion4d{value.rotation}, value.location, value.scale};
}
}

void apply_settings(settings::Settings const& settings, LevelSimInitData& target) {
    target.clock_settings = settings.tick;
    target.lasers = settings.laser_projectiles;
    target.collision.overlap_response = settings.collision_grid;

    target.fighters = settings.fighters;
    target.capital_ships = settings.capital_ships;
    target.turrets = settings.turrets;
    target.spinners = settings.tube_spinners;

    auto const& geometry{settings.entity_geometry};
    auto set_bounds = [&](EntityType type, settings::MeshBounds const& bounds) {
        target.collision.entity_bounds.set_centre(type, bounds.centre);
        target.collision.entity_bounds.set_half_extents(type, bounds.half_extents);
    };
    set_bounds(EntityType::PlayerShip, geometry.player_ship);
    set_bounds(EntityType::CapitalShip, geometry.capital_ship);
    set_bounds(EntityType::Fighter, geometry.fighter);
    set_bounds(EntityType::Turret, geometry.turret);
    set_bounds(EntityType::TubeSpinner, geometry.tube_spinner);
    target.capital_ships.fighter_spawn_slots_relative_transforms.clear();
    for (auto const& slot : geometry.fighter_launch_slots) {
        target.capital_ships.fighter_spawn_slots_relative_transforms.push_back(transform(slot));
    }
    target.fighters.fire_point_distance = geometry.fighter_fire_point_distance;
    target.turrets.fire_point_offset = geometry.turret_fire_point;
    target.spinners.fire_point_offsets.clear();
    for (auto const& point : geometry.spinner_fire_points) {
        target.spinners.fire_point_offsets.push_back(
            {Vector3f{point.location}, Rotator3f{transform(point).rotator()}});
    }

    target.collision.grid_spec = {
        collision::calculate_grid_dimensions(settings.collision_grid.level_size,
                                             settings.collision_grid.cell_size),
        settings.collision_grid.cell_size};
    if (target.player) {
        apply_settings(settings, *target.player);
    }
}

void apply_settings(settings::Settings const& settings, player::PlayerSpawnData& target) {
    auto const& geometry{settings.entity_geometry};
    target.body_transform = transform(geometry.player_body);
    target.left_socket = transform(geometry.player_left_socket);
    target.middle_socket = transform(geometry.player_middle_socket);
    target.right_socket = transform(geometry.player_right_socket);
    target.config = settings.player_ship;
    target.health = ShipHealth{settings.player_ship.health};
    target.laser_mode = settings.player_ship.laser_mode;
    target.laser_fire_rate = settings.player_ship.laser_fire_rate;
}
}
