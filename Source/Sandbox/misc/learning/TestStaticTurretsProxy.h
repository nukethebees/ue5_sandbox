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

    auto get_batch_actor() const -> ATestStaticTurrets const* { return batch_actor; }
    auto get_team() const { return team; }
  protected:
    void OnConstruction(FTransform const& transform) override;

#if WITH_EDITOR
    UFUNCTION(CallInEditor, Category = "Proxy")
    void save_configuration_to_asset();
    UFUNCTION(CallInEditor, Category = "Proxy")
    void apply_asset_configuration();
    UFUNCTION(CallInEditor, Category = "Proxy")
    void apply_asset_configuration_to_all_instances();
#endif

    UPROPERTY(EditAnywhere, Category = "Proxy")
    TObjectPtr<UTestStaticTurretsConfig> actor_config{nullptr};

    UPROPERTY(EditAnywhere, Category = "Proxy")
    TObjectPtr<UStaticMeshComponent> mesh{nullptr};

    UPROPERTY(EditAnywhere, Category = "Proxy")
    TObjectPtr<UCapsuleComponent> collision{nullptr};

    UPROPERTY(EditAnywhere, Category = "Proxy")
    TObjectPtr<USphereComponent> detection{nullptr};

    UPROPERTY(EditAnywhere, Category = "Proxy")
    TObjectPtr<UArrowComponent> fire_point{nullptr};

    UPROPERTY(EditAnywhere, Category = "Proxy")
    TObjectPtr<ATestStaticTurrets> batch_actor{nullptr};

    UPROPERTY(EditAnywhere, Category = "Proxy")
    ETestTeam team{ETestTeam::neutral};
};
