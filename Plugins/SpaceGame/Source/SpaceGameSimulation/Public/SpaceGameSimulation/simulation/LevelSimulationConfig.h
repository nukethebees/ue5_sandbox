#pragma once

#include <CoreMinimal.h>
#include <sandbox/simulation/simulation_config.h>
#include <SandboxCoreEngine/SpeedResponse.h>
#include <SpaceGameSimulation/combat/lasers/AttackDistanceBand.h>
#include <SpaceGameSimulation/ships/common/BarrelRoll.h>

using FSimulationLaserWeaponConfig = ml::simulation::LaserWeaponConfig;
using FLaserSimulationConfig = ml::simulation::LaserSimulationConfig;
using FOverlapResponseConfig = ml::simulation::OverlapResponseConfig;

struct SPACEGAMESIMULATION_API FPlayerSimulationConfig {
    float thrust_energy_max{1.f};
    FSpeedResponses speed_responses{};
    float cruise_speed{12000.f};
    float thrust_recharge_time{7.f};
    float boost_depletion_time{4.f};
    float boost_speed{30000.f};
    float boost_forward_speed_addition_multiplier{2.f};
    float brake_depletion_time{6.f};
    float brake_speed{1000.f};
    float rotation_speed{60.f};
    float pitch_angle_max{30.f};
    float pitch_speed{3.f};
    float yaw_angle_max{30.f};
    float yaw_speed{3.f};
    float turn_bank_angle_max{30.f};
    float turn_bank_speed{2.f};
    float manual_bank_angle_max{90.f};
    float manual_bank_speed{5.f};
    FBarrelRollConfig barrel_roll_config;
    float auto_level_speed{10.f};
    float auto_level_roll_delay{1.f};
    float lateral_adjustment_speed{5000.f};
    float vertical_adjustment_speed{5000.f};
    float planar_lateral_trim_speed{3000.f};
    float planar_vertical_trim_speed{3000.f};
    float forward_velocity_trim_fraction{0.05f};
    FSimulationLaserWeaponConfig laser{};
    float laser_lock_on_transition_delay{1.f};
    float laser_lock_on_distance{10000.f};
};

struct SPACEGAMESIMULATION_API FCapitalSimulationConfig {
    float spawn_delay{5.f};
    int32 fighter_spawn_slots{0};
    TArray<FTransform> fighter_spawn_slots_relative_transforms;
    int32 max_health{5000};
};

struct SPACEGAMESIMULATION_API FFighterSimulationConfig {
    int32 max_live_fighters{2000};
    float fire_dot_product_threshold{0.95f};
    float speed{2000.f};
    float turn_speed_unitless{1.f};
    float avoidance_clear_update_frequency{2.f};
    float avoidance_update_frequency{5.f};
    float avoidance_active_update_frequency{12.f};
    float avoidance_immediate_update_frequency{30.f};
    float avoidance_lookahead_time{1.f};
    float avoidance_clearance_buffer{100.f};
    float separation_radius{2000.f};
    float separation_strength{1.f};
    float steering_memory_duration{0.75f};
    int32 dense_traffic_neighbour_threshold{4};
    FSimulationLaserWeaponConfig laser{};
    int32 health{50};
    float attack_retry_cooldown{0.15f};
    float attack_engagement_threshold{5000.f};
    float attack_reposition_frequency{10.f};
    FAttackDistanceBand attack_distance_band;
    float arrival_distance{500.f};
    float los_check_buffer{100.f};
    float awareness_radius{10000.f};
    float awareness_scan_frequency{6.f};
    float minimum_opportunistic_intercept_deviation_dot_product{0.5f};
};

struct SPACEGAMESIMULATION_API FTurretSimulationConfig {
    int32 search_slice_size{64};
    float detection_radius{3000.f};
    float target_refresh_frequency{5.f};
    FTransform fire_point_offset{FTransform::Identity};
    FSimulationLaserWeaponConfig laser{};
    int32 max_health{20};
};

struct SPACEGAMESIMULATION_API FSpinnerSimulationConfig {
    TArray<FTransform> fire_point_offsets;
    float yaw_rotation_speed_degrees{66.f};
    FSimulationLaserWeaponConfig laser{};
};
