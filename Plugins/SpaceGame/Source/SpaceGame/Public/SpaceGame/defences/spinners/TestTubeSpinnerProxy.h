#pragma once

#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestTubeSpinnerProxy.generated.h"

class UStaticMeshComponent;
class UCapsuleComponent;
class USphereComponent;
class USceneComponent;
class UArrowComponent;

UCLASS()
class ATestTubeSpinnerProxy : public AActor {
  public:
    GENERATED_BODY()

    ATestTubeSpinnerProxy();

    void set_level_config_asset(USpaceGameLevelConfig* const config) noexcept {
        level_config_asset = config;
    }

  private:
    void OnConstruction(FTransform const& transform) override;

#if WITH_EDITOR
    UFUNCTION(CallInEditor, Category = "Proxy|Add Points")
    void add_fire_point();
    UFUNCTION(CallInEditor, Category = "Proxy|Remove Points")
    void remove_all_fire_points();
    UFUNCTION(CallInEditor, Category = "Proxy|Remove Points")
    void remove_fire_point();

    UFUNCTION(CallInEditor, Category = "Proxy|Load Asset")
    void apply_asset_configuration();
    UFUNCTION(CallInEditor, Category = "Proxy|Load Asset")
    void apply_asset_configuration_to_all_instances();

    UFUNCTION(CallInEditor, Category = "Proxy|Position Points")
    void position_fire_points();
#endif
    void add_fire_points(int32 const n);
    void remove_fire_points(int32 const n);
    void face_fire_points_away_from_mesh();

    UPROPERTY(VisibleInstanceOnly, Category = "Proxy")
    TObjectPtr<USpaceGameLevelConfig> level_config_asset{nullptr};

    UPROPERTY(EditAnywhere, Category = "Proxy")
    TObjectPtr<UStaticMeshComponent> mesh{nullptr};

    UPROPERTY(EditAnywhere, Category = "Proxy")
    TArray<TObjectPtr<UArrowComponent>> fire_points{};
};
