#pragma once

#include "SpaceGamePresentation/entities/TestTeam.h"
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestStaticTurretsProxy.generated.h"

class UStaticMeshComponent;
class UCapsuleComponent;
class USphereComponent;
class USceneComponent;
class UArrowComponent;

namespace ioj {
UCLASS()
class ATestStaticTurretsProxy : public AActor {
  public:
    GENERATED_BODY()

    ATestStaticTurretsProxy();

    auto get_team() const { return team; }
    void set_level_config_asset(USpaceGameLevelConfig* const config) noexcept {
        level_config_asset = config;
    }
    void set_team(ETestTeam const new_team) noexcept { team = new_team; }

  protected:
    void OnConstruction(FTransform const& transform) override;
    void configure_component(UPrimitiveComponent& component);
    void apply_actor_configuration();

#if WITH_EDITOR
    UFUNCTION(CallInEditor, Category = "Proxy")
    void apply_asset_configuration();
    UFUNCTION(CallInEditor, Category = "Proxy")
    void apply_asset_configuration_to_all_instances();
#endif

    UPROPERTY(VisibleInstanceOnly, Category = "Proxy")
    TObjectPtr<USpaceGameLevelConfig> level_config_asset{nullptr};

    UPROPERTY(EditAnywhere, Category = "Proxy")
    TObjectPtr<UStaticMeshComponent> mesh{nullptr};

    UPROPERTY(EditAnywhere, Category = "Proxy")
    TObjectPtr<UCapsuleComponent> collision{nullptr};

    UPROPERTY(EditAnywhere, Category = "Proxy")
    TObjectPtr<USphereComponent> detection{nullptr};

    UPROPERTY(EditAnywhere, Category = "Proxy")
    TObjectPtr<UArrowComponent> fire_point{nullptr};

    UPROPERTY(EditAnywhere, Category = "Level Authoring")
    ETestTeam team{ETestTeam::White};

  public:
    UPROPERTY(EditAnywhere, Category = "Level Authoring", meta = (ClampMin = "0.0", Units = "s"))
    double spawn_time_seconds{};

    UPROPERTY(EditAnywhere, Category = "Level Authoring", meta = (ClampMin = "1"))
    TOptional<int32> health;

    UPROPERTY(EditAnywhere, Category = "Level Authoring", meta = (ClampMin = "0"))
    TOptional<int32> laser_damage;
};

} // namespace ioj
