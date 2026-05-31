#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestStaticTurrets.generated.h"

class UInstancedStaticMeshComponent;

class ATestStaticTurretsProxy;
class UTestStaticTurretsConfig;

UCLASS()
class ATestStaticTurrets : public AActor {
  public:
    GENERATED_BODY()

    ATestStaticTurrets();

    void Tick(float dt) override;
  protected:
    void OnConstruction(FTransform const& transform) override;
    void BeginPlay() override;

    void configure_ismc();

    UPROPERTY(EditAnywhere, Category = "Laser")
    TObjectPtr<UTestStaticTurretsConfig> turrets_config{nullptr};

    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> instances;
};
