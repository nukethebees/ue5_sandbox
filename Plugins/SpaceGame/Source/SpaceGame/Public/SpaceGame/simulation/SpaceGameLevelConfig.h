#pragma once

#include <SpaceGameRendering/SparkBurstStyle.h>
#include <SpaceGameRendering/SparkRendererSettings.h>

#include <SpaceGamePresentation/presentation/LevelPresentationSettings.h>
#include <SpaceGamePresentation/support/DrawDebugConfig.h>
#include <SpaceGameSimulation/combat/lasers/AttackDistanceBand.h>
#include <SpaceGameSimulation/ships/common/BarrelRoll.h>

#include <SandboxCoreEngine/collision_settings.h>
#include <SandboxCoreEngine/SpeedResponse.h>

#include <CoreMinimal.h>
#include <Engine/DataAsset.h>
#include <Engine/EngineTypes.h>

#include <SpaceGamePresentation/presentation/LevelActorSettings.h>

#include "SpaceGameLevelConfig.generated.h"

class AActor;
class ATestCapitalShipProxy;
class ATestStaticTurretsProxy;
class ATestSpaceShip;
class ASpaceGamePlayerController;
class UMaterialInterface;
class UNiagaraSystem;
class UStaticMesh;
class UTestTeamVisualData;

USTRUCT(BlueprintType)
struct SPACEGAME_API FScenarioClassConfig {
    GENERATED_BODY()

    FScenarioClassConfig();

    UPROPERTY(EditAnywhere, Category = "Classes")
    TSubclassOf<ASpaceGamePlayerController> player_controller_class{nullptr};

    UPROPERTY(EditAnywhere, Category = "Classes")
    TSubclassOf<ATestSpaceShip> player_ship_class{nullptr};

    UPROPERTY(EditAnywhere, Category = "Classes")
    TSubclassOf<ATestCapitalShipProxy> capital_ship_proxy_class{nullptr};

    UPROPERTY(EditAnywhere, Category = "Classes")
    TSubclassOf<ATestStaticTurretsProxy> static_turret_proxy_class{nullptr};
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
    auto get_visual_config() const -> FLevelVisualConfig {
        return {player_ship,
                laser_projectiles,
                capital_ships,
                fighters,
                turrets,
                tube_spinners,
                laser_debug_drawer,
                laser_debug_shapes,
                capital_debug_shapes,
                fighter_debug_targets,
                fighter_debug_locations,
                turret_debug_targets,
                turret_debug_entities,
                entity_overlay,
                radar,
                sparks};
    }
    [[nodiscard]] auto is_valid(bool require_presentation = true) const noexcept -> bool;
    void get_validation_errors(TArray<FString>& errors, bool require_presentation = true) const;
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

    UPROPERTY(EditAnywhere, Category = "Debug")
    FDrawDebugConfig laser_debug_drawer;

    UPROPERTY(EditAnywhere, Category = "Debug")
    bool laser_debug_shapes{false};

    UPROPERTY(EditAnywhere, Category = "Debug")
    bool capital_debug_shapes{false};

    UPROPERTY(EditAnywhere, Category = "Debug")
    bool fighter_debug_targets{false};

    UPROPERTY(EditAnywhere, Category = "Debug")
    bool fighter_debug_locations{false};

    UPROPERTY(EditAnywhere, Category = "Debug")
    bool turret_debug_targets{false};

    UPROPERTY(EditAnywhere, Category = "Debug")
    bool turret_debug_entities{false};

    UPROPERTY(EditAnywhere, Category = "UI", meta = (ShowOnlyInnerProperties))
    FEntityOverlaySettings entity_overlay;

    UPROPERTY(EditAnywhere, Category = "UI", meta = (ShowOnlyInnerProperties))
    FRadarSettings radar;

    UPROPERTY(EditAnywhere, Category = "Sparks", meta = (ShowOnlyInnerProperties))
    FSparkRendererSettings sparks;
};
