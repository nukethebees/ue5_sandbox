#pragma once

#include "TestEntity.h"
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
class ATestStaticTurretsProxy
    : public AActor
    , public ITestEntity {
  public:
    GENERATED_BODY()

    ATestStaticTurretsProxy();

    auto get_team() const { return team; }
    void set_actor_config(UTestStaticTurretsConfig* const new_config) noexcept {
        actor_config = new_config;
    }

    // ITestEntity
    auto get_entity_handle() const noexcept -> FRegistryEntityHandle override {
        return entity_handle;
    }
    void set_entity_handle(FRegistryEntityHandle const h) noexcept { entity_handle = h; }
#if WITH_EDITOR
    void set_test_name(FName const new_test_name) noexcept { test_name = new_test_name; }
    auto get_test_name() const noexcept -> FName override { return test_name; }
#endif
  protected:
    void OnConstruction(FTransform const& transform) override;
    void configure_component(UPrimitiveComponent& component);

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
    ETestTeam team{ETestTeam::White};

#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere, Category = "Test")
    FName test_name{NAME_None};
#endif

    FRegistryEntityHandle entity_handle;
};
