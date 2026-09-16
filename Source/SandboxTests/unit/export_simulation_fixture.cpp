#include <CQTest.h>
#include <format>
#include <fstream>
#include <HAL/FileManager.h>
#include <iterator>
#include <limits>
#include <Misc/Paths.h>
#include <SandboxTests/support/SimulationTestAssets.h>
#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/WorldlessSimulationTest.h>
#include <type_traits>

namespace ml::fixture_export {
template <typename Output, typename T>
void write_value(Output& output, T const value) {
    if constexpr (std::is_floating_point_v<T>) {
        output = std::format_to(output, "{:.{}e}", value, std::numeric_limits<T>::max_digits10);
        if constexpr (std::is_same_v<T, float>) {
            output = std::format_to(output, "f");
        }
    } else {
        output = std::format_to(output, "{}", value);
    }
}

template <typename T>
void write(std::ostream& out, char const* path, T const value) {
    auto output{std::ostreambuf_iterator<char>{out}};
    output = std::format_to(output, "    {} = ", path);
    if constexpr (std::is_enum_v<T>) {
        output =
            std::format_to(output, "static_cast<decltype({})>({})", path, static_cast<int>(value));
    } else {
        write_value(output, value);
    }
    std::format_to(output, ";\n");
}

template <typename Vector>
void write_vector3(std::ostream& out, char const* function, int const index, Vector const value) {
    auto output{std::ostreambuf_iterator<char>{out}};
    std::format_to(output,
                   "    {}({}, {{{{{:.9e}f, {:.9e}f, {:.9e}f}}}});\n",
                   function,
                   index,
                   value.X,
                   value.Y,
                   value.Z);
}

void transform(std::ostream& out, std::string const& path, ::ioj::sim::Transform3d const& value) {
    auto output{std::ostreambuf_iterator<char>{out}};
    std::format_to(output,
                   "    {} = {{{{{:.17e}, {:.17e}, {:.17e}, {:.17e}}}, "
                   "{{{:.17e}, {:.17e}, {:.17e}}}, "
                   "{{{:.17e}, {:.17e}, {:.17e}}}}};\n",
                   path,
                   value.rotation.x,
                   value.rotation.y,
                   value.rotation.z,
                   value.rotation.w,
                   value.location.x,
                   value.location.y,
                   value.location.z,
                   value.scale.x,
                   value.scale.y,
                   value.scale.z);
}

void transform_components(std::ostream& out,
                          std::string const& path,
                          ::ioj::sim::Transform3d const& value) {
    write(out, (path + ".location.x").c_str(), value.location.x);
    write(out, (path + ".location.y").c_str(), value.location.y);
    write(out, (path + ".location.z").c_str(), value.location.z);
    write(out, (path + ".rotation.x").c_str(), value.rotation.x);
    write(out, (path + ".rotation.y").c_str(), value.rotation.y);
    write(out, (path + ".rotation.z").c_str(), value.rotation.z);
    write(out, (path + ".rotation.w").c_str(), value.rotation.w);
    write(out, (path + ".scale.x").c_str(), value.scale.x);
    write(out, (path + ".scale.y").c_str(), value.scale.y);
    write(out, (path + ".scale.z").c_str(), value.scale.z);
}

void vector3f(std::ostream& out, char const* path, ::ioj::sim::Vector3f const value) {
    auto output{std::ostreambuf_iterator<char>{out}};
    std::format_to(
        output, "    {} = {{{{{:.9e}f, {:.9e}f, {:.9e}f}}}};\n", path, value.X, value.Y, value.Z);
}

}
TEST_CLASS(SimulationFixtureExport, "Sandbox.FixtureTools")
{
    TEST_METHOD(ExportConvertedDefaults)
    {
        ml::FSoftTestAssertions checks{};
        checks.test_runner = TestRunner;
        auto const* config{ml::get_default_level_config(checks)};
        ASSERT_THAT(IsNotNull(config));
        auto const data{ml::make_worldless_simulation_test_data(*config)};
        auto const player{ml::make_worldless_player_spawn(*config)};
        auto const output_path{FPaths::ConvertRelativePathToFull(
            FPaths::ProjectDir() / TEXT(".local/simulation_fixture.cpp"))};
        ASSERT_THAT(IsTrue(IFileManager::Get().MakeDirectory(*FPaths::GetPath(output_path), true)));
        std::ofstream out{TCHAR_TO_UTF8(*output_path)};
        ASSERT_THAT(IsTrue(out.is_open()));
        out << "// Captured from DA_FT_soa_entities_LevelConfig via Unreal's production "
               "conversion.\n"
               "#include \"simulation_fixture.h\"\n"
               "namespace ml::simulation_tests {\n"
               "auto make_fixture() -> SimulationFixture {\n"
               "    SimulationFixture fixture;\n"
               "    auto& data{fixture.data};\n"
               "    auto& player{fixture.player};\n";
        ml::fixture_export::write(
            out, "data.lasers.n_preallocated_instances", data.lasers.n_preallocated_instances);
        ml::fixture_export::write(out, "data.lasers.collision_jobs", data.lasers.collision_jobs);
        ml::fixture_export::write(out,
                                  "data.overlap_response.damage_per_overlap_detection",
                                  data.overlap_response.damage_per_overlap_detection);
        ml::fixture_export::write(
            out, "data.fighters.max_live_fighters", data.fighters.max_live_fighters);
        ml::fixture_export::write(out,
                                  "data.fighters.fire_dot_product_threshold",
                                  data.fighters.fire_dot_product_threshold);
        ml::fixture_export::write(out, "data.fighters.speed", data.fighters.speed);
        ml::fixture_export::write(
            out, "data.fighters.turn_speed_unitless", data.fighters.turn_speed_unitless);
        ml::fixture_export::write(out,
                                  "data.fighters.avoidance_clear_update_frequency",
                                  data.fighters.avoidance_clear_update_frequency);
        ml::fixture_export::write(out,
                                  "data.fighters.avoidance_update_frequency",
                                  data.fighters.avoidance_update_frequency);
        ml::fixture_export::write(out,
                                  "data.fighters.avoidance_active_update_frequency",
                                  data.fighters.avoidance_active_update_frequency);
        ml::fixture_export::write(out,
                                  "data.fighters.avoidance_immediate_update_frequency",
                                  data.fighters.avoidance_immediate_update_frequency);
        ml::fixture_export::write(
            out, "data.fighters.avoidance_lookahead_time", data.fighters.avoidance_lookahead_time);
        ml::fixture_export::write(out,
                                  "data.fighters.avoidance_clearance_buffer",
                                  data.fighters.avoidance_clearance_buffer);
        ml::fixture_export::write(
            out, "data.fighters.separation_radius", data.fighters.separation_radius);
        ml::fixture_export::write(
            out, "data.fighters.separation_strength", data.fighters.separation_strength);
        ml::fixture_export::write(
            out, "data.fighters.steering_memory_duration", data.fighters.steering_memory_duration);
        ml::fixture_export::write(out,
                                  "data.fighters.dense_traffic_neighbour_threshold",
                                  data.fighters.dense_traffic_neighbour_threshold);
        ml::fixture_export::write(out, "data.fighters.health", data.fighters.health);
        ml::fixture_export::write(
            out, "data.fighters.attack_retry_cooldown", data.fighters.attack_retry_cooldown);
        ml::fixture_export::write(out,
                                  "data.fighters.attack_engagement_threshold",
                                  data.fighters.attack_engagement_threshold);
        ml::fixture_export::write(out,
                                  "data.fighters.attack_reposition_frequency",
                                  data.fighters.attack_reposition_frequency);
        ml::fixture_export::write(
            out, "data.fighters.arrival_distance", data.fighters.arrival_distance);
        ml::fixture_export::write(
            out, "data.fighters.los_check_buffer", data.fighters.los_check_buffer);
        ml::fixture_export::write(
            out, "data.fighters.awareness_radius", data.fighters.awareness_radius);
        ml::fixture_export::write(
            out, "data.fighters.awareness_scan_frequency", data.fighters.awareness_scan_frequency);
        ml::fixture_export::write(
            out,
            "data.fighters.minimum_opportunistic_intercept_deviation_dot_product",
            data.fighters.minimum_opportunistic_intercept_deviation_dot_product);
        ml::fixture_export::write(
            out, "data.capital_ships.spawn_delay", data.capital_ships.spawn_delay);
        ml::fixture_export::write(
            out, "data.capital_ships.fighter_spawn_slots", data.capital_ships.fighter_spawn_slots);
        ml::fixture_export::write(
            out, "data.capital_ships.max_health", data.capital_ships.max_health);
        ml::fixture_export::write(
            out, "data.turrets.search_slice_size", data.turrets.search_slice_size);
        ml::fixture_export::write(
            out, "data.turrets.detection_radius", data.turrets.detection_radius);
        ml::fixture_export::write(
            out, "data.turrets.target_refresh_frequency", data.turrets.target_refresh_frequency);
        ml::fixture_export::write(out, "data.turrets.max_health", data.turrets.max_health);
        ml::fixture_export::write(out,
                                  "data.spinners.yaw_rotation_speed_degrees",
                                  data.spinners.yaw_rotation_speed_degrees);
        ml::fixture_export::write(
            out, "player.config.thrust_energy_max", player.config.thrust_energy_max);
        ml::fixture_export::write(out, "player.config.cruise_speed", player.config.cruise_speed);
        ml::fixture_export::write(
            out, "player.config.thrust_recharge_time", player.config.thrust_recharge_time);
        ml::fixture_export::write(
            out, "player.config.boost_depletion_time", player.config.boost_depletion_time);
        ml::fixture_export::write(out, "player.config.boost_speed", player.config.boost_speed);
        ml::fixture_export::write(out,
                                  "player.config.boost_forward_speed_addition_multiplier",
                                  player.config.boost_forward_speed_addition_multiplier);
        ml::fixture_export::write(
            out, "player.config.brake_depletion_time", player.config.brake_depletion_time);
        ml::fixture_export::write(out, "player.config.brake_speed", player.config.brake_speed);
        ml::fixture_export::write(
            out, "player.config.rotation_speed", player.config.rotation_speed);
        ml::fixture_export::write(
            out, "player.config.pitch_angle_max", player.config.pitch_angle_max);
        ml::fixture_export::write(out, "player.config.pitch_speed", player.config.pitch_speed);
        ml::fixture_export::write(out, "player.config.yaw_angle_max", player.config.yaw_angle_max);
        ml::fixture_export::write(out, "player.config.yaw_speed", player.config.yaw_speed);
        ml::fixture_export::write(
            out, "player.config.turn_bank_angle_max", player.config.turn_bank_angle_max);
        ml::fixture_export::write(
            out, "player.config.turn_bank_speed", player.config.turn_bank_speed);
        ml::fixture_export::write(
            out, "player.config.manual_bank_angle_max", player.config.manual_bank_angle_max);
        ml::fixture_export::write(
            out, "player.config.manual_bank_speed", player.config.manual_bank_speed);
        ml::fixture_export::write(
            out, "player.config.auto_level_speed", player.config.auto_level_speed);
        ml::fixture_export::write(
            out, "player.config.auto_level_roll_delay", player.config.auto_level_roll_delay);
        ml::fixture_export::write(
            out, "player.config.lateral_adjustment_speed", player.config.lateral_adjustment_speed);
        ml::fixture_export::write(out,
                                  "player.config.vertical_adjustment_speed",
                                  player.config.vertical_adjustment_speed);
        ml::fixture_export::write(out,
                                  "player.config.planar_lateral_trim_speed",
                                  player.config.planar_lateral_trim_speed);
        ml::fixture_export::write(out,
                                  "player.config.planar_vertical_trim_speed",
                                  player.config.planar_vertical_trim_speed);
        ml::fixture_export::write(out,
                                  "player.config.forward_velocity_trim_fraction",
                                  player.config.forward_velocity_trim_fraction);
        ml::fixture_export::write(out,
                                  "player.config.laser_lock_on_transition_delay",
                                  player.config.laser_lock_on_transition_delay);
        ml::fixture_export::write(
            out, "player.config.laser_lock_on_distance", player.config.laser_lock_on_distance);
        ml::fixture_export::write(out, "data.fighters.laser.damage", data.fighters.laser.damage);
        ml::fixture_export::write(
            out, "data.fighters.laser.projectile_speed", data.fighters.laser.projectile_speed);
        ml::fixture_export::write(
            out, "data.fighters.laser.max_distance", data.fighters.laser.max_distance);
        ml::fixture_export::write(
            out, "data.fighters.laser.fire_cooldown", data.fighters.laser.fire_cooldown);
        ml::fixture_export::write(out, "data.turrets.laser.damage", data.turrets.laser.damage);
        ml::fixture_export::write(
            out, "data.turrets.laser.projectile_speed", data.turrets.laser.projectile_speed);
        ml::fixture_export::write(
            out, "data.turrets.laser.max_distance", data.turrets.laser.max_distance);
        ml::fixture_export::write(
            out, "data.turrets.laser.fire_cooldown", data.turrets.laser.fire_cooldown);
        ml::fixture_export::write(out, "data.spinners.laser.damage", data.spinners.laser.damage);
        ml::fixture_export::write(
            out, "data.spinners.laser.projectile_speed", data.spinners.laser.projectile_speed);
        ml::fixture_export::write(
            out, "data.spinners.laser.max_distance", data.spinners.laser.max_distance);
        ml::fixture_export::write(
            out, "data.spinners.laser.fire_cooldown", data.spinners.laser.fire_cooldown);
        ml::fixture_export::write(out, "player.config.laser.damage", player.config.laser.damage);
        ml::fixture_export::write(
            out, "player.config.laser.projectile_speed", player.config.laser.projectile_speed);
        ml::fixture_export::write(
            out, "player.config.laser.max_distance", player.config.laser.max_distance);
        ml::fixture_export::write(
            out, "player.config.laser.fire_cooldown", player.config.laser.fire_cooldown);
        ml::fixture_export::write(out,
                                  "data.fighters.attack_distance_band.minimum_ratio",
                                  data.fighters.attack_distance_band.minimum_ratio);
        ml::fixture_export::write(out,
                                  "data.fighters.attack_distance_band.desired_ratio",
                                  data.fighters.attack_distance_band.desired_ratio);
        ml::fixture_export::write(out,
                                  "data.fighters.attack_distance_band.maximum_ratio",
                                  data.fighters.attack_distance_band.maximum_ratio);
        ml::fixture_export::write(out,
                                  "player.config.speed_responses.boost.settling_time",
                                  player.config.speed_responses.boost.settling_time);
        ml::fixture_export::write(out,
                                  "player.config.speed_responses.boost.damping_ratio",
                                  player.config.speed_responses.boost.damping_ratio);
        ml::fixture_export::write(out,
                                  "player.config.speed_responses.brake.settling_time",
                                  player.config.speed_responses.brake.settling_time);
        ml::fixture_export::write(out,
                                  "player.config.speed_responses.brake.damping_ratio",
                                  player.config.speed_responses.brake.damping_ratio);
        ml::fixture_export::write(out,
                                  "player.config.speed_responses.slowing_to_cruise.settling_time",
                                  player.config.speed_responses.slowing_to_cruise.settling_time);
        ml::fixture_export::write(out,
                                  "player.config.speed_responses.slowing_to_cruise.damping_ratio",
                                  player.config.speed_responses.slowing_to_cruise.damping_ratio);
        ml::fixture_export::write(
            out,
            "player.config.speed_responses.accelerating_to_cruise.settling_time",
            player.config.speed_responses.accelerating_to_cruise.settling_time);
        ml::fixture_export::write(
            out,
            "player.config.speed_responses.accelerating_to_cruise.damping_ratio",
            player.config.speed_responses.accelerating_to_cruise.damping_ratio);
        ml::fixture_export::write(
            out, "data.fighter_fire_point_distance", data.fighter_fire_point_distance);
        ml::fixture_export::write(
            out, "data.frame_memory_capacity_bytes", data.frame_memory_capacity_bytes);
        ml::fixture_export::write(
            out, "data.clock_settings.tick_rate", data.clock_settings.tick_rate);
        ml::fixture_export::write(
            out, "data.clock_settings.time_scale", data.clock_settings.time_scale);
        ml::fixture_export::write(
            out, "data.clock_settings.tick_period", data.clock_settings.tick_period);
        ml::fixture_export::write(
            out, "data.clock_settings.accumulator", data.clock_settings.accumulator);
        out << "    data.grid_dimensions = {" << data.grid_dimensions.x << ", "
            << data.grid_dimensions.y << ", " << data.grid_dimensions.z << "};\n";
        ml::fixture_export::vector3f(out, "data.cell_size", data.cell_size);
        ml::fixture_export::vector3f(
            out, "data.turrets.fire_point_offset", data.turrets.fire_point_offset);
        ml::fixture_export::write(out, "player.team", player.team);
        ml::fixture_export::write(out, "player.flight_mode", player.flight_mode);
        ml::fixture_export::write(out, "player.control_mode", player.control_mode);
        ml::fixture_export::write(out, "player.laser_mode", player.laser_mode);
        ml::fixture_export::write(out, "player.laser_fire_rate", player.laser_fire_rate);
        ml::fixture_export::write(out, "player.health.health", player.health.health);
        ml::fixture_export::write(out, "player.health.max_health", player.health.max_health);
        ml::fixture_export::transform(out, "player.transform", player.transform);
        ml::fixture_export::transform(out, "player.body_transform", player.body_transform);
        ml::fixture_export::transform(out, "player.left_socket", player.left_socket);
        ml::fixture_export::transform(out, "player.right_socket", player.right_socket);
        ml::fixture_export::transform(out, "player.middle_socket", player.middle_socket);
        auto const slot_count{data.capital_ships.fighter_spawn_slots_relative_transforms.size()};
        out << "    data.capital_ships.fighter_spawn_slots_relative_transforms.resize("
            << slot_count << ");\n";
        for (std::size_t i{}; i < slot_count; ++i) {
            ml::fixture_export::transform_components(
                out,
                "data.capital_ships.fighter_spawn_slots_relative_transforms[" + std::to_string(i) +
                    "]",
                data.capital_ships.fighter_spawn_slots_relative_transforms[i]);
        }
        auto const point_count{data.spinners.fire_point_offsets.size()};
        out << "    data.spinners.fire_point_offsets.resize(" << point_count << ");\n";
        for (std::size_t i{}; i < point_count; ++i) {
            auto const& point{data.spinners.fire_point_offsets[i]};
            auto const path{"data.spinners.fire_point_offsets[" + std::to_string(i) + "]"};
            ml::fixture_export::write(out, (path + ".location.X").c_str(), point.location.X);
            ml::fixture_export::write(out, (path + ".location.Y").c_str(), point.location.Y);
            ml::fixture_export::write(out, (path + ".location.Z").c_str(), point.location.Z);
            ml::fixture_export::write(
                out, (path + ".rotation.pitch").c_str(), point.rotation.pitch);
            ml::fixture_export::write(out, (path + ".rotation.yaw").c_str(), point.rotation.yaw);
            ml::fixture_export::write(out, (path + ".rotation.roll").c_str(), point.rotation.roll);
        }
        for (int i{}; i < data.entity_bounds.num(); ++i) {
            ml::fixture_export::write_vector3(
                out, "data.entity_bounds.set_centre", i, data.entity_bounds.get_centre(i));
            ml::fixture_export::write_vector3(out,
                                              "data.entity_bounds.set_half_extents",
                                              i,
                                              data.entity_bounds.get_half_extents(i));
        }
        out << "    return fixture;\n}\n}\n";
        out.flush();
        ASSERT_THAT(IsTrue(out.good()));
    }
};
