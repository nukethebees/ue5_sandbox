#pragma once

#include <SpaceGame/combat/lasers/AttackDistanceBand.h>
#include <SpaceGame/ships/common/BarrelRoll.h>
#include <SpaceGame/support/DrawDebugConfig.h>

#include <SandboxCoreEngine/collision_settings.h>
#include <SandboxGameShared/players/SpeedResponse.h>

#include <CoreMinimal.h>
#include <Engine/DataAsset.h>
#include <Engine/EngineTypes.h>

#include "SpaceGameLevelConfig.generated.h"

class ADelayedNiagaraSpawner;
class AActor;
class ATestCapitalShipFighters;
class ATestCapitalShipProxy;
class ATestCapitalShips;
class ATestLasers;
class ATestSpaceShip;
class ASpaceGamePlayerController;
class ATestStaticTurrets;
class ATestTubeSpinners;
class UMaterialInterface;
class UNiagaraSystem;
class USandboxVisualLoggerStyle;
class UStaticMesh;
class UTestTeamVisualData;

UENUM(BlueprintType)
enum class ECapitalShipMainExplosionDelayMode : uint8 {
    AfterSmallExplosions,
    Absolute,
};

USTRUCT(BlueprintType)
struct SPACEGAME_API FLaserWeaponConfig {
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
struct SPACEGAME_API FScenarioClassConfig {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Classes")
    TSubclassOf<ASpaceGamePlayerController> player_controller_class{nullptr};

    UPROPERTY(EditAnywhere, Category = "Classes")
    TSubclassOf<ATestSpaceShip> player_ship_class{nullptr};

    UPROPERTY(EditAnywhere, Category = "Classes")
    TSubclassOf<ATestLasers> lasers_class{nullptr};

    UPROPERTY(EditAnywhere, Category = "Classes")
    TSubclassOf<ATestCapitalShips> capital_ships_class{nullptr};

    UPROPERTY(EditAnywhere, Category = "Classes")
    TSubclassOf<ATestCapitalShipProxy> capital_ship_proxy_class{nullptr};

    UPROPERTY(EditAnywhere, Category = "Classes")
    TSubclassOf<ATestCapitalShipFighters> capital_ship_fighters_class{nullptr};

    UPROPERTY(EditAnywhere, Category = "Classes")
    TSubclassOf<ATestStaticTurrets> turrets_class{nullptr};

    UPROPERTY(EditAnywhere, Category = "Classes")
    TSubclassOf<ATestTubeSpinners> spinners_class{nullptr};

    UPROPERTY(EditAnywhere, Category = "Classes")
    TSubclassOf<ADelayedNiagaraSpawner> niagara_spawner_class{nullptr};
};

USTRUCT(BlueprintType)
struct SPACEGAME_API FPlayerShipConfig {
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

    UPROPERTY(EditAnywhere, Category = "Combat")
    FLaserWeaponConfig laser{};

    UPROPERTY(EditAnywhere, Category = "Combat")
    float laser_lock_on_transition_delay{1.f};

    UPROPERTY(EditAnywhere, Category = "Combat")
    float laser_lock_on_distance{10000.f};
};

USTRUCT(BlueprintType)
struct SPACEGAME_API FLaserProjectileConfig {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UStaticMesh> mesh{nullptr};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UMaterialInterface> material{nullptr};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    float min_cull_distance{0.f};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    float max_cull_distance{50000.f};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UNiagaraSystem> hit_effect{nullptr};
};

USTRUCT(BlueprintType)
struct SPACEGAME_API FCapitalShipConfig {
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

    UPROPERTY(EditDefaultsOnly, Category = "Debug")
    TObjectPtr<USandboxVisualLoggerStyle> visual_logger_style{nullptr};

    UPROPERTY(EditAnywhere, Category = "Proxy")
    float proxy_arrow_size{5.f};
};

USTRUCT(BlueprintType)
struct SPACEGAME_API FFighterConfig {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UStaticMesh> mesh{nullptr};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UTestTeamVisualData> team_visual_data{nullptr};

    UPROPERTY(EditAnywhere, Category = "Movement")
    float speed{2000.f};

    UPROPERTY(EditAnywhere, Category = "Movement")
    float turn_speed_unitless{1.f};

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

    UPROPERTY(EditDefaultsOnly, Category = "Debug")
    TObjectPtr<USandboxVisualLoggerStyle> visual_logger_style{nullptr};
};

USTRUCT(BlueprintType)
struct SPACEGAME_API FTurretConfig {
    GENERATED_BODY()

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
struct SPACEGAME_API FTubeSpinnerConfig {
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

USTRUCT(BlueprintType)
struct SPACEGAME_API FCollisionGridConfig {
    GENERATED_BODY()

    FCollisionGridConfig();

    [[nodiscard]] auto calculate_grid_dimensions() const noexcept -> FIntVector3;
    [[nodiscard]] auto is_valid() const noexcept -> bool;

    UPROPERTY(EditAnywhere, Category = "Collision", meta = (Units = "cm"))
    FVector3f grid_size{2000000.f, 2000000.f, 100000.f};

    UPROPERTY(EditAnywhere, Category = "Collision", meta = (Units = "cm"))
    FVector3f cell_size{5000.f, 5000.f, 20000.f};

    UPROPERTY(EditAnywhere, Category = "Collision|Static Geometry")
    TArray<TSubclassOf<AActor>> harvested_collision_actor_classes;

    UPROPERTY(EditAnywhere, Category = "Collision|Static Geometry")
    TArray<TSubclassOf<AActor>> omitted_collision_actor_classes;

    UPROPERTY(EditAnywhere, Category = "Collision|Visualization")
    bool show_grid{false};

    UPROPERTY(EditAnywhere, Category = "Collision|Visualization", meta = (ClampMin = "0.1"))
    float line_thickness{1.f};

    UPROPERTY(EditAnywhere, Category = "Collision|Visualization")
    FLinearColor line_colour{0.f, 1.f, 1.f, 1.f};
};

UCLASS(BlueprintType)
class SPACEGAME_API USpaceGameLevelConfig : public UDataAsset {
    GENERATED_BODY()
  public:
    [[nodiscard]] auto is_valid() const noexcept -> bool;
    void get_validation_errors(TArray<FString>& errors) const;
    void get_validation_warnings(TArray<FString>& warnings) const;

#if WITH_EDITOR
    EDataValidationResult IsDataValid(FDataValidationContext& context) const override;
#endif

    UPROPERTY(EditAnywhere, Category = "Level")
    FScenarioClassConfig classes;

    UPROPERTY(EditAnywhere, Category = "Level")
    FPlayerShipConfig player_ship;

    UPROPERTY(EditAnywhere, Category = "Level")
    FLaserProjectileConfig laser_projectiles;

    UPROPERTY(EditAnywhere, Category = "Level")
    FCapitalShipConfig capital_ships;

    UPROPERTY(EditAnywhere, Category = "Level")
    FFighterConfig fighters;

    UPROPERTY(EditAnywhere, Category = "Level")
    FTurretConfig turrets;

    UPROPERTY(EditAnywhere, Category = "Level")
    FTubeSpinnerConfig tube_spinners;

    UPROPERTY(EditAnywhere, Category = "Level")
    FCollisionGridConfig collision_grid;
};
