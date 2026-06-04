#pragma once

#include "TestTeam.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestTubeSpinnerProxy.generated.h"

class UStaticMeshComponent;
class UCapsuleComponent;
class USphereComponent;
class USceneComponent;
class UArrowComponent;

class UTestTubeSpinnersConfig;
class ATestStaticTurrets;

UCLASS()
class ATestTubeSpinnerProxy : public AActor {
  public:
    GENERATED_BODY()

    ATestTubeSpinnerProxy();
  protected:
#if WITH_EDITOR
    UFUNCTION(CallInEditor, Category = "Proxy")
    void add_fire_point();
    UFUNCTION(CallInEditor, Category = "Proxy")
    void pop_fire_point();

    UFUNCTION(CallInEditor, Category = "Proxy")
    void save_configuration_to_asset();
    UFUNCTION(CallInEditor, Category = "Proxy")
    void apply_asset_configuration();
    UFUNCTION(CallInEditor, Category = "Proxy")
    void apply_asset_configuration_to_all_instances();
#endif

    UPROPERTY(EditAnywhere, Category = "Proxy")
    TObjectPtr<UTestTubeSpinnersConfig> actor_config{nullptr};

    UPROPERTY(EditAnywhere, Category = "Proxy")
    TObjectPtr<UStaticMeshComponent> mesh{nullptr};

    UPROPERTY(EditAnywhere, Category = "Proxy")
    TArray<TObjectPtr<UArrowComponent>> fire_points{};
};
