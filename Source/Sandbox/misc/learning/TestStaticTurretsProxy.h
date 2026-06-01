#pragma once

#include "TestTeam.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestStaticTurretsProxy.generated.h"

class UStaticMeshComponent;
class UCapsuleComponent;
class USphereComponent;
class USceneComponent;

class UTestStaticTurretsConfig;
class ATestStaticTurrets;

UCLASS()
class ATestStaticTurretsProxy : public AActor {
  public:
    GENERATED_BODY()

    ATestStaticTurretsProxy();

    void Tick(float dt) override;
  protected:
    void OnConstruction(FTransform const& transform) override;
    void BeginPlay() override;

#if WITH_EDITOR
    UFUNCTION(CallInEditor, Category = "Turret")
    void save_configuration_to_asset();
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
    TObjectPtr<USceneComponent> fire_point{nullptr};

    UPROPERTY(EditAnywhere, Category = "Turret")
    TObjectPtr<ATestStaticTurrets> batch_actor{nullptr};

    UPROPERTY(EditAnywhere, Category = "Turret")
    ETestTeam team{ETestTeam::neutral};
};
