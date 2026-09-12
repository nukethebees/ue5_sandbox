#pragma once
#include <CoreMinimal.h>
#include <SandboxCoreEngine/collision_settings.h>
#include <SandboxCoreEngine/SpeedResponse.h>
#include <SpaceGamePresentation/presentation/LevelPresentationSettings.h>
#include <SpaceGamePresentation/support/DrawDebugConfig.h>
#include <SpaceGameRendering/SparkBurstStyle.h>
#include <SpaceGameRendering/SparkRendererSettings.h>
#include <SpaceGameSimulation/combat/lasers/AttackDistanceBand.h>
#include <SpaceGameSimulation/ships/common/BarrelRoll.h>
#include "LevelActorSettings.generated.h"

class UMaterialInterface;
class UNiagaraSystem;
class UStaticMesh;
class UTestTeamVisualData;

UENUM(BlueprintType)
enum class ECapitalShipMainExplosionDelayMode : uint8 {
    AfterSmallExplosions,
    Absolute,
};

USTRUCT(BlueprintType)
struct SPACEGAMEPRESENTATION_API FLaserWeaponConfig {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Combat")
    int32 damage{5};

    UPROPERTY(EditAnywhere, Category = "Combat")
    float projectile_speed{10000.f};

    UPROPERTY(EditAnywhere, Category = "Combat")
    float max_distance{10000.f};

    UPROPERTY(EditAnywhere, Category = "Combat")
    float fire_cooldown{0.33f};
};

USTRUCT(BlueprintType)
struct SPACEGAMEPRESENTATION_API FPlayerShipConfig {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Visuals")
    float boost_effect_colour_intensity{75.f};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    FLinearColor engine_colour{FLinearColor::Blue};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UTestTeamVisualData> team_visual_data{nullptr};

    UPROPERTY(EditAnywhere, Category = "Energy")
    float thrust_energy_max{1.f};

    UPROPERTY(EditAnywhere, Category = "Movement")
    FSpeedResponses speed_responses{};

    UPROPERTY(EditAnywhere, Category = "Movement")
    float cruise_speed{12000.f};

    UPROPERTY(EditAnywhere, Category = "Movement")
    float thrust_recharge_time{7.f};

    UPROPERTY(EditAnywhere, Category = "Movement")
    float boost_depletion_time{4.f};

    UPROPERTY(EditAnywhere, Category = "Movement")
    float boost_speed{30000.f};

    UPROPERTY(EditAnywhere, Category = "Movement")
    float brake_depletion_time{6.f};

    UPROPERTY(EditAnywhere, Category = "Movement")
    float brake_speed{1000.f};

    UPROPERTY(EditAnywhere, Category = "Movement|Steering")
    float rotation_speed{60.f};

    UPROPERTY(EditAnywhere, Category = "Movement|Steering")
    float pitch_angle_max{30.f};

    UPROPERTY(EditAnywhere, Category = "Movement|Steering")
    float pitch_speed{3.f};

    UPROPERTY(EditAnywhere, Category = "Movement|Steering")
    float yaw_angle_max{30.f};

    UPROPERTY(EditAnywhere, Category = "Movement|Steering")
    float yaw_speed{3.f};

    UPROPERTY(EditAnywhere, Category = "Movement|Steering")
    float turn_bank_angle_max{30.f};

    UPROPERTY(EditAnywhere, Category = "Movement|Steering")
    float turn_bank_speed{2.f};

    UPROPERTY(EditAnywhere, Category = "Movement|Steering")
    float manual_bank_angle_max{90.f};

    UPROPERTY(EditAnywhere, Category = "Movement|Steering")
    float manual_bank_speed{5.f};

    UPROPERTY(EditAnywhere, Category = "Movement|Steering")
    FBarrelRollConfig barrel_roll_config;

    UPROPERTY(EditAnywhere, Category = "Movement")
    float auto_level_speed{10.f};

    UPROPERTY(EditAnywhere, Category = "Movement")
    float auto_level_roll_delay{1.f};

    UPROPERTY(EditAnywhere, Category = "Movement")
    float lateral_adjustment_speed{5000.f};

    UPROPERTY(EditAnywhere, Category = "Movement")
    float vertical_adjustment_speed{5000.f};

    UPROPERTY(EditAnywhere, Category = "Movement")
    float planar_lateral_trim_speed{3000.f};

    UPROPERTY(EditAnywhere, Category = "Movement")
    float planar_vertical_trim_speed{3000.f};

    UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float forward_velocity_trim_fraction{0.05f};

    UPROPERTY(EditAnywhere, Category = "Combat")
    FLaserWeaponConfig laser{};

    UPROPERTY(EditAnywhere, Category = "Combat")
    float laser_lock_on_transition_delay{1.f};

    UPROPERTY(EditAnywhere, Category = "Combat")
    float laser_lock_on_distance{10000.f};
};

USTRUCT(BlueprintType)
struct SPACEGAMEPRESENTATION_API FLaserProjectileConfig {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Simulation", meta = (ClampMin = "0"))
    int32 n_preallocated_instances{5000};

    UPROPERTY(EditAnywhere, Category = "Simulation", meta = (ClampMin = "1"))
    int32 collision_jobs{8};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UStaticMesh> mesh{nullptr};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UMaterialInterface> material{nullptr};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    float min_cull_distance{0.f};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    float max_cull_distance{50000.f};

    UPROPERTY(EditAnywhere, Category = "Visuals|Sparks", meta = (ShowOnlyInnerProperties))
    FSparkBurstStyle impact_sparks;
};

USTRUCT(BlueprintType)
struct SPACEGAMEPRESENTATION_API FCapitalShipConfig {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UStaticMesh> mesh{nullptr};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UMaterialInterface> material{nullptr};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UNiagaraSystem> small_death_explosion{nullptr};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    int32 n_small_explosions{6};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    float time_between_explosions{0.1f};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    FVector3f min_small_explosion_range{FVector3f::ZeroVector};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    FVector3f max_small_explosion_range{FVector3f::OneVector};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UNiagaraSystem> main_death_explosion{nullptr};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    ECapitalShipMainExplosionDelayMode main_explosion_delay_mode{
        ECapitalShipMainExplosionDelayMode::AfterSmallExplosions};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    float large_explosion_delay{0.f};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UTestTeamVisualData> team_visual_data{nullptr};

    UPROPERTY(EditAnywhere, Category = "Collision")
    FCollisionSettings collision_settings;

    UPROPERTY(EditAnywhere, Category = "Fighters")
    float spawn_delay{5.f};

    UPROPERTY(EditAnywhere, Category = "Fighters")
    int32 fighter_spawn_slots{6};

    UPROPERTY(EditAnywhere, Category = "Fighters")
    TArray<FTransform> fighter_spawn_slots_relative_transforms;

    UPROPERTY(EditAnywhere, Category = "Health")
    int32 max_health{5000};

    UPROPERTY(EditAnywhere, Category = "Debug")
    FDrawDebugConfig debug_drawer;

    UPROPERTY(EditAnywhere, Category = "Debug")
    FVector debug_status_text_offset{0.0, 0.0, 500.0};

    UPROPERTY(EditAnywhere, Category = "Proxy")
    float proxy_arrow_size{5.f};
};

USTRUCT(BlueprintType)
struct SPACEGAMEPRESENTATION_API FFighterConfig {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "-1", ClampMax = "1"))
    float fire_dot_product_threshold{0.95f};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UStaticMesh> mesh{nullptr};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UTestTeamVisualData> team_visual_data{nullptr};

    UPROPERTY(EditAnywhere, Category = "Movement")
    float speed{2000.f};

    UPROPERTY(EditAnywhere, Category = "Movement")
    float turn_speed_unitless{1.f};

    UPROPERTY(EditAnywhere,
              Category = "Movement|Avoidance",
              meta = (ClampMin = "0.001", Units = "Hz"))
    float avoidance_clear_update_frequency{2.f};

    // This is the nearby-traffic tier frequency.
    UPROPERTY(EditAnywhere,
              Category = "Movement|Avoidance",
              meta = (ClampMin = "0.001", Units = "Hz"))
    float avoidance_update_frequency{5.f};

    UPROPERTY(EditAnywhere,
              Category = "Movement|Avoidance",
              meta = (ClampMin = "0.001", Units = "Hz"))
    float avoidance_active_update_frequency{12.f};

    UPROPERTY(EditAnywhere,
              Category = "Movement|Avoidance",
              meta = (ClampMin = "0.001", Units = "Hz"))
    float avoidance_immediate_update_frequency{30.f};

    UPROPERTY(EditAnywhere, Category = "Movement|Avoidance", meta = (ClampMin = "0.0", Units = "s"))
    float avoidance_lookahead_time{1.f};

    UPROPERTY(EditAnywhere,
              Category = "Movement|Avoidance",
              meta = (ClampMin = "0.0", Units = "cm"))
    float avoidance_clearance_buffer{100.f};

    UPROPERTY(EditAnywhere,
              Category = "Movement|Separation",
              meta = (ClampMin = "0.001", Units = "cm"))
    float separation_radius{2000.f};

    UPROPERTY(EditAnywhere, Category = "Movement|Separation", meta = (ClampMin = "0.0"))
    float separation_strength{1.f};

    UPROPERTY(EditAnywhere,
              Category = "Movement|Separation",
              meta = (ClampMin = "0.0", Units = "s"))
    float steering_memory_duration{0.75f};

    UPROPERTY(EditAnywhere, Category = "Movement|Separation", meta = (ClampMin = "2"))
    int32 dense_traffic_neighbour_threshold{4};

    UPROPERTY(EditAnywhere, Category = "Combat")
    FLaserWeaponConfig laser{};

    UPROPERTY(EditAnywhere, Category = "Health")
    int32 health{50};

    UPROPERTY(EditAnywhere, Category = "Combat")
    float attack_retry_cooldown{0.15f};

    UPROPERTY(EditAnywhere, Category = "Combat")
    float attack_engagement_threshold{5000.f};

    UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "0.001", Units = "Hz"))
    float attack_reposition_frequency{10.f};

    UPROPERTY(EditAnywhere, Category = "Combat")
    FAttackDistanceBand attack_distance_band;

    UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "0.0", Units = "cm"))
    float arrival_distance{500.f};

    UPROPERTY(EditAnywhere, Category = "Combat")
    float los_check_buffer{100.f};

    UPROPERTY(EditAnywhere, Category = "Awareness")
    float awareness_radius{10000.f};

    UPROPERTY(EditAnywhere, Category = "Awareness", meta = (ClampMin = "0.001", Units = "Hz"))
    float awareness_scan_frequency{6.f};

    UPROPERTY(EditAnywhere, Category = "Awareness")
    float minimum_opportunistic_intercept_deviation_dot_product{0.5f};

    UPROPERTY(EditAnywhere, Category = "Debug")
    FDrawDebugConfig debug_drawer;
};

USTRUCT(BlueprintType)
struct SPACEGAMEPRESENTATION_API FTurretConfig {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Simulation", meta = (ClampMin = "1"))
    int32 search_slice_size{64};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UStaticMesh> mesh{nullptr};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UNiagaraSystem> death_effect{nullptr};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    FVector death_effect_offset{FVector::ZeroVector};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    float death_effect_scale{1.f};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UTestTeamVisualData> team_visual_data{nullptr};

    UPROPERTY(EditAnywhere, Category = "Awareness")
    float detection_radius{3000.f};

    UPROPERTY(EditAnywhere, Category = "Awareness", meta = (ClampMin = "0.01"))
    float target_refresh_frequency{5.f};

    UPROPERTY(EditAnywhere, Category = "Combat")
    FTransform fire_point_offset{FTransform::Identity};

    UPROPERTY(EditAnywhere, Category = "Combat")
    FLaserWeaponConfig laser{};

    UPROPERTY(EditAnywhere, Category = "Health")
    int32 max_health{20};

    UPROPERTY(EditAnywhere, Category = "Debug")
    bool show_collision{false};

    UPROPERTY(EditAnywhere, Category = "Debug")
    FDrawDebugConfig debug_drawer;

    UPROPERTY(EditAnywhere, Category = "Debug")
    FVector debug_status_text_offset{0.0, 0.0, 500.0};
};

USTRUCT(BlueprintType)
struct SPACEGAMEPRESENTATION_API FTubeSpinnerConfig {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UStaticMesh> mesh{nullptr};

    UPROPERTY(EditAnywhere, Category = "Combat")
    TArray<FTransform> fire_point_offsets;

    UPROPERTY(EditAnywhere, Category = "Movement")
    float yaw_rotation_speed_degrees{66.f};

    UPROPERTY(EditAnywhere, Category = "Combat")
    FLaserWeaponConfig laser{};

    UPROPERTY(EditAnywhere, Category = "Debug")
    FDrawDebugConfig debug_drawer;
};

struct SPACEGAMEPRESENTATION_API FLevelVisualConfig {
    FPlayerShipConfig player_ship;
    FLaserProjectileConfig laser_projectiles;
    FCapitalShipConfig capital_ships;
    FFighterConfig fighters;
    FTurretConfig turrets;
    FTubeSpinnerConfig tube_spinners;
    FDrawDebugConfig laser_debug_drawer;
    bool laser_debug_shapes{};
    bool capital_debug_shapes{};
    bool fighter_debug_targets{};
    bool fighter_debug_locations{};
    bool turret_debug_targets{};
    bool turret_debug_entities{};
    FEntityOverlaySettings entity_overlay;
    FRadarSettings radar;
    FSparkRendererSettings sparks;
};
