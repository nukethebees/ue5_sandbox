#include <SpaceGame/simulation/SimulationConfigConversion.h>
#include <SpaceGameSimulation/simulation/NativeRotatorTypes.h>
#include <SpaceGameSimulation/simulation/NativeTransformTypes.h>
#include <SpaceGameSimulation/simulation/NativeVectorTypes.h>

auto make_simulation_config(FLaserWeaponConfig const& source) -> ::ioj::sim::LaserWeaponSimConfig {
    ::ioj::sim::LaserWeaponSimConfig result;
    result.damage = source.damage;
    result.projectile_speed = source.projectile_speed;
    result.max_distance = source.max_distance;
    result.fire_cooldown = source.fire_cooldown;
    return result;
}

auto make_simulation_config(FPlayerShipConfig const& source) -> ::ioj::sim::PlayerSimConfig {
    ::ioj::sim::PlayerSimConfig result;
    result.thrust_energy_max = source.thrust_energy_max;
    result.speed_responses = ml::to_native(source.speed_responses);
    result.cruise_speed = source.cruise_speed;
    result.thrust_recharge_time = source.thrust_recharge_time;
    result.boost_depletion_time = source.boost_depletion_time;
    result.boost_speed = source.boost_speed;
    result.boost_forward_speed_addition_multiplier = source.boost_forward_speed_addition_multiplier;
    result.brake_depletion_time = source.brake_depletion_time;
    result.brake_speed = source.brake_speed;
    result.rotation_speed = source.rotation_speed;
    result.pitch_angle_max = source.pitch_angle_max;
    result.pitch_speed = source.pitch_speed;
    result.yaw_angle_max = source.yaw_angle_max;
    result.yaw_speed = source.yaw_speed;
    result.turn_bank_angle_max = source.turn_bank_angle_max;
    result.turn_bank_speed = source.turn_bank_speed;
    result.manual_bank_angle_max = source.manual_bank_angle_max;
    result.manual_bank_speed = source.manual_bank_speed;
    result.auto_level_speed = source.auto_level_speed;
    result.auto_level_roll_delay = source.auto_level_roll_delay;
    result.lateral_adjustment_speed = source.lateral_adjustment_speed;
    result.vertical_adjustment_speed = source.vertical_adjustment_speed;
    result.planar_lateral_trim_speed = source.planar_lateral_trim_speed;
    result.planar_vertical_trim_speed = source.planar_vertical_trim_speed;
    result.forward_velocity_trim_fraction = source.forward_velocity_trim_fraction;
    result.laser = make_simulation_config(source.laser);
    result.laser_lock_on_transition_delay = source.laser_lock_on_transition_delay;
    result.laser_lock_on_distance = source.laser_lock_on_distance;
    return result;
}

auto make_simulation_config(FLaserProjectileConfig const& source) -> ::ioj::sim::LaserSimConfig {
    ::ioj::sim::LaserSimConfig result;
    result.n_preallocated_instances = source.n_preallocated_instances;
    result.collision_jobs = source.collision_jobs;
    return result;
}

auto make_simulation_config(FCapitalShipConfig const& source) -> ::ioj::sim::CapitalShipSimConfig {
    ::ioj::sim::CapitalShipSimConfig result;
    result.spawn_delay = source.spawn_delay;
    result.fighter_spawn_slots = source.fighter_spawn_slots;
    result.fighter_spawn_slots_relative_transforms.reserve(
        source.fighter_spawn_slots_relative_transforms.Num());
    for (auto const& transform : source.fighter_spawn_slots_relative_transforms) {
        result.fighter_spawn_slots_relative_transforms.push_back(ml::to_native(transform));
    }
    result.max_health = source.max_health;
    return result;
}

auto make_simulation_config(FFighterConfig const& source) -> ::ioj::sim::FighterSimConfig {
    ::ioj::sim::FighterSimConfig result;
    result.fire_dot_product_threshold = source.fire_dot_product_threshold;
    result.speed = source.speed;
    result.turn_speed_unitless = source.turn_speed_unitless;
    result.avoidance_clear_update_frequency = source.avoidance_clear_update_frequency;
    result.avoidance_update_frequency = source.avoidance_update_frequency;
    result.avoidance_active_update_frequency = source.avoidance_active_update_frequency;
    result.avoidance_immediate_update_frequency = source.avoidance_immediate_update_frequency;
    result.avoidance_lookahead_time = source.avoidance_lookahead_time;
    result.avoidance_clearance_buffer = source.avoidance_clearance_buffer;
    result.separation_radius = source.separation_radius;
    result.separation_strength = source.separation_strength;
    result.steering_memory_duration = source.steering_memory_duration;
    result.dense_traffic_neighbour_threshold = source.dense_traffic_neighbour_threshold;
    result.laser = make_simulation_config(source.laser);
    result.health = source.health;
    result.attack_retry_cooldown = source.attack_retry_cooldown;
    result.attack_engagement_threshold = source.attack_engagement_threshold;
    result.attack_reposition_frequency = source.attack_reposition_frequency;
    result.attack_distance_band = source.attack_distance_band.to_native();
    result.arrival_distance = source.arrival_distance;
    result.los_check_buffer = source.los_check_buffer;
    result.awareness_radius = source.awareness_radius;
    result.awareness_scan_frequency = source.awareness_scan_frequency;
    result.minimum_opportunistic_intercept_deviation_dot_product =
        source.minimum_opportunistic_intercept_deviation_dot_product;
    return result;
}

auto make_simulation_config(FTurretConfig const& source) -> ::ioj::sim::TurretSimConfig {
    ::ioj::sim::TurretSimConfig result;
    result.search_slice_size = source.search_slice_size;
    result.detection_radius = source.detection_radius;
    result.target_refresh_frequency = source.target_refresh_frequency;
    result.fire_point_offset = ml::to_native(FVector3f{source.fire_point_offset.GetLocation()});
    result.laser = make_simulation_config(source.laser);
    result.max_health = source.max_health;
    return result;
}

auto make_simulation_config(FTubeSpinnerConfig const& source) -> ::ioj::sim::SpinnerSimConfig {
    ::ioj::sim::SpinnerSimConfig result;
    result.fire_point_offsets.reserve(static_cast<std::size_t>(source.fire_point_offsets.Num()));
    for (auto const& offset : source.fire_point_offsets) {
        result.fire_point_offsets.push_back({ml::to_native(FVector3f{offset.GetLocation()}),
                                             ml::to_native(FRotator3f{offset.Rotator()})});
    }
    result.yaw_rotation_speed_degrees = source.yaw_rotation_speed_degrees;
    result.laser = make_simulation_config(source.laser);
    return result;
}
