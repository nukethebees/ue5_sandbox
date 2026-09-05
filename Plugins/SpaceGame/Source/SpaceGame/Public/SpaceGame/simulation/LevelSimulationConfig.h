#pragma once

#include <CoreMinimal.h>
#include <SandboxGameShared/players/SpeedResponse.h>
#include <SpaceGame/combat/lasers/AttackDistanceBand.h>
#include <SpaceGame/entities/TeamColours.h>
#include <SpaceGame/ships/common/BarrelRoll.h>

struct FLaserWeaponConfig;
struct SPACEGAME_API FSimulationLaserWeaponConfig {
    int32 damage{5};
    float projectile_speed{10000.f};
    float max_distance{10000.f};
    float fire_cooldown{0.33f};
};
auto SPACEGAME_API make_simulation_config(FLaserWeaponConfig const& source)
    -> FSimulationLaserWeaponConfig;

struct FPlayerShipConfig;
struct SPACEGAME_API FPlayerSimulationConfig {
    float thrust_energy_max{1.f};
    FSpeedResponses speed_responses{};
    float cruise_speed{12000.f};
    float thrust_recharge_time{7.f};
    float boost_depletion_time{4.f};
    float boost_speed{30000.f};
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
    FSimulationLaserWeaponConfig laser{};
    float laser_lock_on_transition_delay{1.f};
    float laser_lock_on_distance{10000.f};
    FTeamColours team_colours{};
};
auto SPACEGAME_API make_simulation_config(FPlayerShipConfig const& source)
    -> FPlayerSimulationConfig;

struct FLaserProjectileConfig;
struct SPACEGAME_API FLaserSimulationConfig {
    int32 n_preallocated_instances{5000};
    int32 collision_jobs{8};
};
auto SPACEGAME_API make_simulation_config(FLaserProjectileConfig const& source)
    -> FLaserSimulationConfig;

struct FCapitalShipConfig;
struct SPACEGAME_API FCapitalSimulationConfig {
    float spawn_delay{5.f};
    int32 fighter_spawn_slots{0};
    TArray<FTransform> fighter_spawn_slots_relative_transforms;
    int32 max_health{5000};
};
auto SPACEGAME_API make_simulation_config(FCapitalShipConfig const& source)
    -> FCapitalSimulationConfig;

struct FFighterConfig;
struct SPACEGAME_API FFighterSimulationConfig {
    float fire_dot_product_threshold{0.95f};
    float speed{2000.f};
    float turn_speed_unitless{1.f};
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
    FTeamColours team_colours{};
};
auto SPACEGAME_API make_simulation_config(FFighterConfig const& source) -> FFighterSimulationConfig;

struct FTurretConfig;
struct SPACEGAME_API FTurretSimulationConfig {
    int32 search_slice_size{64};
    float detection_radius{3000.f};
    float target_refresh_frequency{5.f};
    FTransform fire_point_offset{FTransform::Identity};
    FSimulationLaserWeaponConfig laser{};
    int32 max_health{20};
    FTeamColours team_colours{};
};
auto SPACEGAME_API make_simulation_config(FTurretConfig const& source) -> FTurretSimulationConfig;

struct FTubeSpinnerConfig;
struct SPACEGAME_API FSpinnerSimulationConfig {
    TArray<FTransform> fire_point_offsets;
    float yaw_rotation_speed_degrees{66.f};
    FSimulationLaserWeaponConfig laser{};
};
auto SPACEGAME_API make_simulation_config(FTubeSpinnerConfig const& source)
    -> FSpinnerSimulationConfig;
