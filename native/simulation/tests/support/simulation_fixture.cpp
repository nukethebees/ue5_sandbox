// Captured from DA_FT_soa_entities_LevelConfig via Unreal's production conversion.
#include "simulation_fixture.h"
namespace ml::simulation_tests {
auto make_fixture() -> SimulationFixture {
    SimulationFixture fixture;
    auto& data{fixture.data};
    auto& player{fixture.player};
    data.lasers.n_preallocated_instances = 50000;
    data.lasers.collision_jobs = 8;
    data.overlap_response.damage_per_overlap_detection = 50;
    data.fighters.max_live_fighters = 2000;
    data.fighters.fire_dot_product_threshold = 9.499999881e-01f;
    data.fighters.speed = 4.500000000e+03f;
    data.fighters.turn_speed_unitless = 1.000000000e+00f;
    data.fighters.avoidance_clear_update_frequency = 2.000000000e+00f;
    data.fighters.avoidance_update_frequency = 5.000000000e+00f;
    data.fighters.avoidance_active_update_frequency = 1.200000000e+01f;
    data.fighters.avoidance_immediate_update_frequency = 3.000000000e+01f;
    data.fighters.avoidance_lookahead_time = 1.000000000e+00f;
    data.fighters.avoidance_clearance_buffer = 1.000000000e+02f;
    data.fighters.separation_radius = 2.000000000e+03f;
    data.fighters.separation_strength = 1.000000000e+00f;
    data.fighters.steering_memory_duration = 7.500000000e-01f;
    data.fighters.dense_traffic_neighbour_threshold = 4;
    data.fighters.health = 50;
    data.fighters.attack_retry_cooldown = 1.500000060e-01f;
    data.fighters.attack_engagement_threshold = 5.000000000e+03f;
    data.fighters.attack_reposition_frequency = 1.000000000e+01f;
    data.fighters.arrival_distance = 5.000000000e+02f;
    data.fighters.los_check_buffer = 1.000000000e+02f;
    data.fighters.awareness_radius = 1.000000000e+04f;
    data.fighters.awareness_scan_frequency = 6.000000000e+00f;
    data.fighters.minimum_opportunistic_intercept_deviation_dot_product = 5.000000000e-01f;
    data.capital_ships.spawn_delay = 5.000000000e+00f;
    data.capital_ships.fighter_spawn_slots = 6;
    data.capital_ships.max_health = 5000;
    data.turrets.search_slice_size = 128;
    data.turrets.detection_radius = 5.000000000e+04f;
    data.turrets.target_refresh_frequency = 5.000000000e+00f;
    data.turrets.max_health = 20;
    data.spinners.yaw_rotation_speed_degrees = 6.600000000e+01f;
    player.config.thrust_energy_max = 1.000000000e+00f;
    player.config.cruise_speed = 6.000000000e+03f;
    player.config.thrust_recharge_time = 7.000000000e+00f;
    player.config.boost_depletion_time = 4.000000000e+00f;
    player.config.boost_speed = 1.200000000e+04f;
    player.config.boost_forward_speed_addition_multiplier = 2.000000000e+00f;
    player.config.brake_depletion_time = 6.000000000e+00f;
    player.config.brake_speed = 3.000000000e+03f;
    player.config.rotation_speed = 6.000000000e+01f;
    player.config.pitch_angle_max = 3.000000000e+01f;
    player.config.pitch_speed = 3.000000000e+00f;
    player.config.yaw_angle_max = 3.000000000e+01f;
    player.config.yaw_speed = 3.000000000e+00f;
    player.config.turn_bank_angle_max = 3.000000000e+01f;
    player.config.turn_bank_speed = 2.000000000e+00f;
    player.config.manual_bank_angle_max = 9.000000000e+01f;
    player.config.manual_bank_speed = 5.000000000e+00f;
    player.config.auto_level_speed = 3.000000000e+01f;
    player.config.auto_level_roll_delay = 1.000000000e+00f;
    player.config.lateral_adjustment_speed = 5.000000000e+03f;
    player.config.vertical_adjustment_speed = 5.000000000e+03f;
    player.config.planar_lateral_trim_speed = 3.000000000e+03f;
    player.config.planar_vertical_trim_speed = 3.000000000e+03f;
    player.config.forward_velocity_trim_fraction = 5.000000075e-02f;
    player.config.laser_lock_on_transition_delay = 1.000000000e+00f;
    player.config.laser_lock_on_distance = 1.000000000e+04f;
    data.fighters.laser.damage = 10;
    data.fighters.laser.projectile_speed = 1.000000000e+04f;
    data.fighters.laser.max_distance = 4.000000000e+04f;
    data.fighters.laser.fire_cooldown = 3.300000131e-01f;
    data.turrets.laser.damage = 5;
    data.turrets.laser.projectile_speed = 1.500000000e+04f;
    data.turrets.laser.max_distance = 4.000000000e+04f;
    data.turrets.laser.fire_cooldown = 3.300000131e-01f;
    data.spinners.laser.damage = 2;
    data.spinners.laser.projectile_speed = 1.000000000e+04f;
    data.spinners.laser.max_distance = 1.000000000e+04f;
    data.spinners.laser.fire_cooldown = 3.000000119e-01f;
    player.config.laser.damage = 50;
    player.config.laser.projectile_speed = 4.000000000e+04f;
    player.config.laser.max_distance = 3.000000000e+04f;
    player.config.laser.fire_cooldown = 1.500000060e-01f;
    data.fighters.attack_distance_band.minimum_ratio = 4.000000060e-01f;
    data.fighters.attack_distance_band.desired_ratio = 5.000000000e-01f;
    data.fighters.attack_distance_band.maximum_ratio = 6.000000238e-01f;
    player.config.speed_responses.boost.settling_time = 3.000000000e+00f;
    player.config.speed_responses.boost.damping_ratio = 5.000000000e-01f;
    player.config.speed_responses.brake.settling_time = 3.000000000e+00f;
    player.config.speed_responses.brake.damping_ratio = 5.000000000e-01f;
    player.config.speed_responses.slowing_to_cruise.settling_time = 3.000000000e+00f;
    player.config.speed_responses.slowing_to_cruise.damping_ratio = 5.000000000e-01f;
    player.config.speed_responses.accelerating_to_cruise.settling_time = 1.000000000e+00f;
    player.config.speed_responses.accelerating_to_cruise.damping_ratio = 7.500000000e-01f;
    data.capital_radius = 9.514702148e+03f;
    data.fighter_radius = 1.591244385e+03f;
    data.turret_radius = 9.241118774e+02f;
    data.spinner_radius = 2.549509735e+02f;
    data.fighter_fire_point_distance = 1.807294556e+03f;
    data.frame_memory_capacity_bytes = 16777216;
    data.clock_settings.tick_rate = 6.00000000000000000e+01;
    data.clock_settings.time_scale = 1.00000000000000000e+00;
    data.clock_settings.tick_period = 0.00000000000000000e+00;
    data.clock_settings.accumulator = 0.00000000000000000e+00;
    data.grid_dimensions.x = 400;
    data.grid_dimensions.y = 400;
    data.grid_dimensions.z = 5;
    data.cell_size.X = 5.000000000e+03f;
    data.turrets.fire_point_offset.X = 0.000000000e+00f;
    data.cell_size.Y = 5.000000000e+03f;
    data.turrets.fire_point_offset.Y = 0.000000000e+00f;
    data.cell_size.Z = 2.000000000e+04f;
    data.turrets.fire_point_offset.Z = 1.560000000e+03f;
    player.team = static_cast<decltype(player.team)>(0);
    player.collision_radius = 2.442084503e+02f;
    player.flight_mode = static_cast<decltype(player.flight_mode)>(1);
    player.control_mode = static_cast<decltype(player.control_mode)>(0);
    player.laser_mode = static_cast<decltype(player.laser_mode)>(0);
    player.laser_fire_rate = static_cast<decltype(player.laser_fire_rate)>(2);
    player.health.health = 1000;
    player.health.max_health = 1000;
    player.transform.location.x = 0.00000000000000000e+00;
    player.transform.location.y = 0.00000000000000000e+00;
    player.transform.location.z = 0.00000000000000000e+00;
    player.transform.rotation.x = 0.00000000000000000e+00;
    player.transform.rotation.y = 0.00000000000000000e+00;
    player.transform.rotation.z = 0.00000000000000000e+00;
    player.transform.rotation.w = 1.00000000000000000e+00;
    player.transform.scale.x = 1.00000000000000000e+00;
    player.transform.scale.y = 1.00000000000000000e+00;
    player.transform.scale.z = 1.00000000000000000e+00;
    player.body_transform.location.x = 0.00000000000000000e+00;
    player.body_transform.location.y = 0.00000000000000000e+00;
    player.body_transform.location.z = 0.00000000000000000e+00;
    player.body_transform.rotation.x = 0.00000000000000000e+00;
    player.body_transform.rotation.y = 0.00000000000000000e+00;
    player.body_transform.rotation.z = 0.00000000000000000e+00;
    player.body_transform.rotation.w = 1.00000000000000000e+00;
    player.body_transform.scale.x = 1.00000000000000000e+00;
    player.body_transform.scale.y = 1.00000000000000000e+00;
    player.body_transform.scale.z = 1.00000000000000000e+00;
    player.left_socket.location.x = 2.00000000000000000e+02;
    player.left_socket.location.y = -1.29999999999998295e+02;
    player.left_socket.location.z = 0.00000000000000000e+00;
    player.left_socket.rotation.x = 3.07504227942209978e-15;
    player.left_socket.rotation.y = -3.77489497438432342e-08;
    player.left_socket.rotation.z = 8.14603398069378503e-08;
    player.left_socket.rotation.w = 9.99999999999995781e-01;
    player.left_socket.scale.x = 9.99999977648258320e-01;
    player.left_socket.scale.y = 9.99999977648258653e-01;
    player.left_socket.scale.z = 9.99999977648258542e-01;
    player.right_socket.location.x = 2.00000000000000000e+02;
    player.right_socket.location.y = 1.39999999999998153e+02;
    player.right_socket.location.z = 0.00000000000000000e+00;
    player.right_socket.rotation.x = 3.07504227942209978e-15;
    player.right_socket.rotation.y = -3.77489497438432342e-08;
    player.right_socket.rotation.z = 8.14603398069378503e-08;
    player.right_socket.rotation.w = 9.99999999999995781e-01;
    player.right_socket.scale.x = 9.99999977648258320e-01;
    player.right_socket.scale.y = 9.99999977648258653e-01;
    player.right_socket.scale.z = 9.99999977648258542e-01;
    player.middle_socket.location.x = 2.00000000000000000e+02;
    player.middle_socket.location.y = 0.00000000000000000e+00;
    player.middle_socket.location.z = 0.00000000000000000e+00;
    player.middle_socket.rotation.x = 3.07504227942209978e-15;
    player.middle_socket.rotation.y = -3.77489497438432342e-08;
    player.middle_socket.rotation.z = 8.14603398069378503e-08;
    player.middle_socket.rotation.w = 9.99999999999995781e-01;
    player.middle_socket.scale.x = 9.99999977648258320e-01;
    player.middle_socket.scale.y = 9.99999977648258653e-01;
    player.middle_socket.scale.z = 9.99999977648258542e-01;
    data.capital_ships.fighter_spawn_slots_relative_transforms.resize(6);
    data.capital_ships.fighter_spawn_slots_relative_transforms[0].location.x =
        -3.00000000000000000e+02;
    data.capital_ships.fighter_spawn_slots_relative_transforms[0].location.y =
        7.00000000000000000e+03;
    data.capital_ships.fighter_spawn_slots_relative_transforms[0].location.z =
        -2.90000000000000000e+03;
    data.capital_ships.fighter_spawn_slots_relative_transforms[0].rotation.x =
        0.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[0].rotation.y =
        -0.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[0].rotation.z =
        7.07106781186547573e-01;
    data.capital_ships.fighter_spawn_slots_relative_transforms[0].rotation.w =
        7.07106781186547684e-01;
    data.capital_ships.fighter_spawn_slots_relative_transforms[0].scale.x = 1.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[0].scale.y = 1.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[0].scale.z = 1.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[1].location.x =
        3.30000000000000000e+03;
    data.capital_ships.fighter_spawn_slots_relative_transforms[1].location.y =
        7.00000000000000000e+03;
    data.capital_ships.fighter_spawn_slots_relative_transforms[1].location.z =
        -2.90000000000000000e+03;
    data.capital_ships.fighter_spawn_slots_relative_transforms[1].rotation.x =
        0.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[1].rotation.y =
        -0.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[1].rotation.z =
        7.07106781186547573e-01;
    data.capital_ships.fighter_spawn_slots_relative_transforms[1].rotation.w =
        7.07106781186547684e-01;
    data.capital_ships.fighter_spawn_slots_relative_transforms[1].scale.x = 1.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[1].scale.y = 1.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[1].scale.z = 1.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[2].location.x =
        6.90000000000000000e+03;
    data.capital_ships.fighter_spawn_slots_relative_transforms[2].location.y =
        7.00000000000000000e+03;
    data.capital_ships.fighter_spawn_slots_relative_transforms[2].location.z =
        -2.90000000000000000e+03;
    data.capital_ships.fighter_spawn_slots_relative_transforms[2].rotation.x =
        0.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[2].rotation.y =
        -0.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[2].rotation.z =
        7.07106781186547573e-01;
    data.capital_ships.fighter_spawn_slots_relative_transforms[2].rotation.w =
        7.07106781186547684e-01;
    data.capital_ships.fighter_spawn_slots_relative_transforms[2].scale.x = 1.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[2].scale.y = 1.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[2].scale.z = 1.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[3].location.x =
        -3.00000000000000000e+02;
    data.capital_ships.fighter_spawn_slots_relative_transforms[3].location.y =
        7.00000000000000000e+03;
    data.capital_ships.fighter_spawn_slots_relative_transforms[3].location.z =
        7.00000000000000000e+02;
    data.capital_ships.fighter_spawn_slots_relative_transforms[3].rotation.x =
        0.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[3].rotation.y =
        -0.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[3].rotation.z =
        7.07106781186547573e-01;
    data.capital_ships.fighter_spawn_slots_relative_transforms[3].rotation.w =
        7.07106781186547684e-01;
    data.capital_ships.fighter_spawn_slots_relative_transforms[3].scale.x = 1.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[3].scale.y = 1.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[3].scale.z = 1.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[4].location.x =
        3.30000000000000000e+03;
    data.capital_ships.fighter_spawn_slots_relative_transforms[4].location.y =
        7.00000000000000000e+03;
    data.capital_ships.fighter_spawn_slots_relative_transforms[4].location.z =
        7.00000000000000000e+02;
    data.capital_ships.fighter_spawn_slots_relative_transforms[4].rotation.x =
        0.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[4].rotation.y =
        -0.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[4].rotation.z =
        7.07106781186547573e-01;
    data.capital_ships.fighter_spawn_slots_relative_transforms[4].rotation.w =
        7.07106781186547684e-01;
    data.capital_ships.fighter_spawn_slots_relative_transforms[4].scale.x = 1.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[4].scale.y = 1.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[4].scale.z = 1.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[5].location.x =
        6.90000000000000000e+03;
    data.capital_ships.fighter_spawn_slots_relative_transforms[5].location.y =
        7.00000000000000000e+03;
    data.capital_ships.fighter_spawn_slots_relative_transforms[5].location.z =
        7.00000000000000000e+02;
    data.capital_ships.fighter_spawn_slots_relative_transforms[5].rotation.x =
        0.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[5].rotation.y =
        -0.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[5].rotation.z =
        7.07106781186547573e-01;
    data.capital_ships.fighter_spawn_slots_relative_transforms[5].rotation.w =
        7.07106781186547684e-01;
    data.capital_ships.fighter_spawn_slots_relative_transforms[5].scale.x = 1.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[5].scale.y = 1.00000000000000000e+00;
    data.capital_ships.fighter_spawn_slots_relative_transforms[5].scale.z = 1.00000000000000000e+00;
    data.spinners.fire_point_offsets.resize(9);
    data.spinners.fire_point_offsets[0].location.X = 1.000000000e+02f;
    data.spinners.fire_point_offsets[0].location.Y = 0.000000000e+00f;
    data.spinners.fire_point_offsets[0].location.Z = 0.000000000e+00f;
    data.spinners.fire_point_offsets[0].rotation.pitch = 0.000000000e+00f;
    data.spinners.fire_point_offsets[0].rotation.yaw = 0.000000000e+00f;
    data.spinners.fire_point_offsets[0].rotation.roll = -0.000000000e+00f;
    data.spinners.fire_point_offsets[1].location.X = 5.000000000e+01f;
    data.spinners.fire_point_offsets[1].location.Y = 7.000000000e+01f;
    data.spinners.fire_point_offsets[1].location.Z = 1.500000000e+02f;
    data.spinners.fire_point_offsets[1].rotation.pitch = 0.000000000e+00f;
    data.spinners.fire_point_offsets[1].rotation.yaw = 5.446232224e+01f;
    data.spinners.fire_point_offsets[1].rotation.roll = -0.000000000e+00f;
    data.spinners.fire_point_offsets[2].location.X = -5.000000000e+01f;
    data.spinners.fire_point_offsets[2].location.Y = 8.000000000e+01f;
    data.spinners.fire_point_offsets[2].location.Z = 2.400000000e+02f;
    data.spinners.fire_point_offsets[2].rotation.pitch = 0.000000000e+00f;
    data.spinners.fire_point_offsets[2].rotation.yaw = 1.220053864e+02f;
    data.spinners.fire_point_offsets[2].rotation.roll = -0.000000000e+00f;
    data.spinners.fire_point_offsets[3].location.X = -1.000000000e+02f;
    data.spinners.fire_point_offsets[3].location.Y = 0.000000000e+00f;
    data.spinners.fire_point_offsets[3].location.Z = 3.300000000e+02f;
    data.spinners.fire_point_offsets[3].rotation.pitch = 0.000000000e+00f;
    data.spinners.fire_point_offsets[3].rotation.yaw = 1.800000000e+02f;
    data.spinners.fire_point_offsets[3].rotation.roll = -0.000000000e+00f;
    data.spinners.fire_point_offsets[4].location.X = -7.000000000e+01f;
    data.spinners.fire_point_offsets[4].location.Y = -8.000000000e+01f;
    data.spinners.fire_point_offsets[4].location.Z = 4.200000000e+02f;
    data.spinners.fire_point_offsets[4].rotation.pitch = 0.000000000e+00f;
    data.spinners.fire_point_offsets[4].rotation.yaw = -1.311859283e+02f;
    data.spinners.fire_point_offsets[4].rotation.roll = 0.000000000e+00f;
    data.spinners.fire_point_offsets[5].location.X = 0.000000000e+00f;
    data.spinners.fire_point_offsets[5].location.Y = -8.000000000e+01f;
    data.spinners.fire_point_offsets[5].location.Z = 4.800000000e+02f;
    data.spinners.fire_point_offsets[5].rotation.pitch = 0.000000000e+00f;
    data.spinners.fire_point_offsets[5].rotation.yaw = -9.000000000e+01f;
    data.spinners.fire_point_offsets[5].rotation.roll = 0.000000000e+00f;
    data.spinners.fire_point_offsets[6].location.X = 8.000000000e+01f;
    data.spinners.fire_point_offsets[6].location.Y = -8.000000000e+01f;
    data.spinners.fire_point_offsets[6].location.Z = 4.200000000e+02f;
    data.spinners.fire_point_offsets[6].rotation.pitch = 0.000000000e+00f;
    data.spinners.fire_point_offsets[6].rotation.yaw = -4.500000000e+01f;
    data.spinners.fire_point_offsets[6].rotation.roll = 0.000000000e+00f;
    data.spinners.fire_point_offsets[7].location.X = 1.000000000e+02f;
    data.spinners.fire_point_offsets[7].location.Y = -5.000000000e+01f;
    data.spinners.fire_point_offsets[7].location.Z = 3.400000000e+02f;
    data.spinners.fire_point_offsets[7].rotation.pitch = 0.000000000e+00f;
    data.spinners.fire_point_offsets[7].rotation.yaw = -2.656505203e+01f;
    data.spinners.fire_point_offsets[7].rotation.roll = 0.000000000e+00f;
    data.spinners.fire_point_offsets[8].location.X = 1.100000000e+02f;
    data.spinners.fire_point_offsets[8].location.Y = 4.000000000e+01f;
    data.spinners.fire_point_offsets[8].location.Z = 2.900000000e+02f;
    data.spinners.fire_point_offsets[8].rotation.pitch = 0.000000000e+00f;
    data.spinners.fire_point_offsets[8].rotation.yaw = 1.998310661e+01f;
    data.spinners.fire_point_offsets[8].rotation.roll = -0.000000000e+00f;
    data.entity_bounds.centre_xs[0] = 0.000000000e+00f;
    data.entity_bounds.centre_ys[0] = 0.000000000e+00f;
    data.entity_bounds.centre_zs[0] = 0.000000000e+00f;
    data.entity_bounds.half_extent_xs[0] = 0.000000000e+00f;
    data.entity_bounds.half_extent_ys[0] = 0.000000000e+00f;
    data.entity_bounds.half_extent_zs[0] = 0.000000000e+00f;
    data.entity_bounds.centre_xs[1] = 0.000000000e+00f;
    data.entity_bounds.centre_ys[1] = 0.000000000e+00f;
    data.entity_bounds.centre_zs[1] = 5.675825806e+02f;
    data.entity_bounds.half_extent_xs[1] = 3.356489258e+02f;
    data.entity_bounds.half_extent_ys[1] = 3.356489258e+02f;
    data.entity_bounds.half_extent_zs[1] = 6.843348389e+02f;
    data.entity_bounds.centre_xs[2] = 4.786312500e+03f;
    data.entity_bounds.centre_ys[2] = 2.441406250e-04f;
    data.entity_bounds.centre_zs[2] = -1.687307007e+03f;
    data.entity_bounds.half_extent_xs[2] = 8.918416016e+03f;
    data.entity_bounds.half_extent_ys[2] = 5.000000488e+03f;
    data.entity_bounds.half_extent_zs[2] = 5.077661133e+03f;
    data.entity_bounds.centre_xs[3] = 0.000000000e+00f;
    data.entity_bounds.centre_ys[3] = 0.000000000e+00f;
    data.entity_bounds.centre_zs[3] = 0.000000000e+00f;
    data.entity_bounds.half_extent_xs[3] = 1.720659668e+03f;
    data.entity_bounds.half_extent_ys[3] = 1.021299377e+03f;
    data.entity_bounds.half_extent_zs[3] = 4.383157349e+02f;
    data.entity_bounds.centre_xs[4] = 0.000000000e+00f;
    data.entity_bounds.centre_ys[4] = 0.000000000e+00f;
    data.entity_bounds.centre_zs[4] = 2.500000000e+02f;
    data.entity_bounds.half_extent_xs[4] = 5.000000000e+01f;
    data.entity_bounds.half_extent_ys[4] = 5.000000000e+01f;
    data.entity_bounds.half_extent_zs[4] = 2.500000000e+02f;
    return fixture;
}
}
