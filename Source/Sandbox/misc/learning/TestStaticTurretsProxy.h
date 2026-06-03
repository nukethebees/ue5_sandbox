#pragma once

#include "TestTeam.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestStaticTurretsProxy.generated.h"

class UStaticMeshComponent;
class UCapsuleComponent;
class USphereComponent;
class USceneComponent;
class UArrowComponent;

class UTestStaticTurretsConfig;
class ATestStaticTurrets;

UCLASS()
class ATestStaticTurretsProxy : public AActor {
  public:
    GENERATED_BODY()

    ATestStaticTurretsProxy();

    void Tick(float dt) override;

    auto get_batch_actor() const -> ATestStaticTurrets const* { return batch_actor; }
    auto get_team() const { return team; }
  protected:
    void OnConstruction(FTransform const& transform) override;
    void BeginPlay() override;

#if WITH_EDITOR
    UFUNCTION(CallInEditor, Category = "Ship")
    void save_configuration_to_asset();
    UFUNCTION(CallInEditor, Category = "Ship")
    void apply_asset_configuration();
    UFUNCTION(CallInEditor, Category = "Ship")
    void apply_asset_configuration_to_all_instances();
#endif

    UPROPERTY(EditAnywhere, Category = "Turret")
    TObjectPtr<UTestStaticTurretsConfig> actor_config{nullptr};

    UPROPERTY(EditAnywhere, Category = "Turret")
    TObjectPtr<UStaticMeshComponent> mesh{nullptr};

    UPROPERTY(EditAnywhere, Category = "Turret")
    TObjectPtr<UCapsuleComponent> collision{nullptr};

    UPROPERTY(EditAnywhere, Category = "Turret")
    TObjectPtr<USphereComponent> detection{nullptr};

    UPROPERTY(EditAnywhere, Category = "Turret")
    TObjectPtr<UArrowComponent> fire_point{nullptr};

    UPROPERTY(EditAnywhere, Category = "Turret")
    TObjectPtr<ATestStaticTurrets> batch_actor{nullptr};

    UPROPERTY(EditAnywhere, Category = "Turret")
    ETestTeam team{ETestTeam::neutral};
};
