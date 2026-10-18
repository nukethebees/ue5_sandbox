#pragma once
#include <ioj/sim/capital_ships/capital_ship_sim_config.h>
#include <ioj/sim/fighters/fighter_sim_config.h>
#include <ioj/sim/lasers/laser_sim_config.h>
#include <ioj/sim/levels/simulation_settings.h>
#include <ioj/sim/player/player_sim_config.h>
#include <ioj/sim/spinners/spinner_sim_config.h>
#include <ioj/sim/turrets/turret_sim_config.h>
#include <SpaceGamePresentation/config/BarrelRollConfig.h>
#include <SpaceGamePresentation/presentation/LevelPresentationSettings.h>
#include <SpaceGamePresentation/support/DrawDebugConfig.h>
#include <SpaceGameRendering/SparkBurstStyle.h>
#include <SpaceGameRendering/SparkRendererSettings.h>

#include <SandboxCoreEngine/collision_settings.h>
#include <SandboxShaders/SpaceDust/SpaceDustComponent.h>

#include <CoreMinimal.h>

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
struct SPACEGAMEPRESENTATION_API FPlayerShipConfig {
    GENERATED_BODY()

    ioj::sim::PlayerSimConfig simulation;
    ioj::sim::PlayerInitialSettings initial;

    float boost_effect_colour_intensity{75.f};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    FLinearColor engine_colour{FLinearColor::Blue};

    UPROPERTY(EditAnywhere, Category = "Visuals", meta = (ShowOnlyInnerProperties))
    FSpaceDustSettings space_dust;

    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UTestTeamVisualData> team_visual_data{nullptr};

    FBarrelRollConfig barrel_roll_config;
};

USTRUCT(BlueprintType)
struct SPACEGAMEPRESENTATION_API FLaserProjectileConfig {
    GENERATED_BODY()

    ioj::sim::LaserSimConfig simulation;

    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UStaticMesh> mesh{nullptr};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UMaterialInterface> material{nullptr};

    float min_cull_distance{0.f};

    float max_cull_distance{50000.f};

    UPROPERTY(EditAnywhere, Category = "Visuals|Sparks", meta = (ShowOnlyInnerProperties))
    FSparkBurstStyle impact_sparks;
};

USTRUCT(BlueprintType)
struct SPACEGAMEPRESENTATION_API FCapitalShipConfig {
    GENERATED_BODY()

    ioj::sim::CapitalShipSimConfig simulation;

    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UStaticMesh> mesh{nullptr};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UMaterialInterface> material{nullptr};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UNiagaraSystem> small_death_explosion{nullptr};

    int32 n_small_explosions{6};

    float time_between_explosions{0.1f};

    FVector3f min_small_explosion_range{FVector3f::ZeroVector};

    FVector3f max_small_explosion_range{FVector3f::OneVector};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UNiagaraSystem> main_death_explosion{nullptr};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    ECapitalShipMainExplosionDelayMode main_explosion_delay_mode{
        ECapitalShipMainExplosionDelayMode::AfterSmallExplosions};

    float large_explosion_delay{0.f};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UTestTeamVisualData> team_visual_data{nullptr};

    UPROPERTY(EditAnywhere, Category = "Collision")
    FCollisionSettings collision_settings;

    UPROPERTY(EditAnywhere, Category = "Fighters")
    TArray<FTransform> fighter_spawn_slots_relative_transforms;

    UPROPERTY(EditAnywhere, Category = "Debug")
    FDrawDebugConfig debug_drawer;

    UPROPERTY(EditAnywhere, Category = "Debug")
    FVector debug_status_text_offset{0.0, 0.0, 500.0};

    float proxy_arrow_size{5.f};
};

USTRUCT(BlueprintType)
struct SPACEGAMEPRESENTATION_API FFighterConfig {
    GENERATED_BODY()

    ioj::sim::FighterSimConfig simulation;

    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UStaticMesh> mesh{nullptr};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UTestTeamVisualData> team_visual_data{nullptr};

    UPROPERTY(EditAnywhere, Category = "Debug")
    FDrawDebugConfig debug_drawer;
};

USTRUCT(BlueprintType)
struct SPACEGAMEPRESENTATION_API FTurretConfig {
    GENERATED_BODY()

    ioj::sim::TurretSimConfig simulation;

    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UStaticMesh> mesh{nullptr};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UNiagaraSystem> death_effect{nullptr};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    FVector death_effect_offset{FVector::ZeroVector};

    float death_effect_scale{1.f};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UTestTeamVisualData> team_visual_data{nullptr};

    UPROPERTY(EditAnywhere, Category = "Combat")
    FTransform fire_point_offset{FTransform::Identity};

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

    ioj::sim::SpinnerSimConfig simulation;

    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UStaticMesh> mesh{nullptr};

    UPROPERTY(EditAnywhere, Category = "Combat")
    TArray<FTransform> fire_point_offsets;

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
    bool fighter_debug_targets{};
    bool fighter_debug_locations{};
    bool turret_debug_targets{};
    bool turret_debug_entities{};
    FEntityOverlaySettings entity_overlay;
    FRadarSettings radar;
    FSparkRendererSettings sparks;
};
