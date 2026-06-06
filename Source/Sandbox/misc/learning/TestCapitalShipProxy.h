#pragma once

#include "TestTeam.h"

#include <SandboxCore/generation_index.h>

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestCapitalShipProxy.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UArrowComponent;

class UTestCapitalShipsConfig;

UCLASS()
class ATestCapitalShipProxy : public AActor {
    GENERATED_BODY()
  public:
    ATestCapitalShipProxy();
  protected:
    void OnConstruction(FTransform const& transform) override;
  public:
#if WITH_EDITOR
    UFUNCTION(CallInEditor, Category = "Ship")
    void apply_asset_configuration();
    UFUNCTION(CallInEditor, Category = "Ship")
    void apply_asset_configuration_to_all_instances();
    UFUNCTION(CallInEditor, Category = "Ship")
    void save_configuration_to_asset();
#endif

    UPROPERTY(EditAnywhere, Category = "Ship")
    TObjectPtr<UTestCapitalShipsConfig> ship_config{nullptr};

    UPROPERTY(EditAnywhere, Category = "Ship")
    TObjectPtr<UStaticMeshComponent> mesh{nullptr};

    UPROPERTY(EditAnywhere, Category = "Ship")
    TObjectPtr<UBoxComponent> collision_box{nullptr};

    UPROPERTY(EditAnywhere, Category = "Ship")
    TArray<TObjectPtr<UArrowComponent>> fighter_spawn_slots;

    UPROPERTY(EditAnywhere, Category = "Ship")
    TObjectPtr<ATestCapitalShipProxy> target_ship{nullptr};

    UPROPERTY(EditAnywhere, Category = "Ship")
    ETestTeam team{ETestTeam::neutral};
};
