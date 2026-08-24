#pragma once

#include "SpaceGame/entities/TestEntity.h"
#include "SpaceGame/entities/TestTeam.h"

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
class ATestTubeSpinnerProxy
    : public AActor
    , public ITestEntity {
  public:
    GENERATED_BODY()

    ATestTubeSpinnerProxy();

    auto get_initial_active_fire_point() const { return initial_active_fire_point; }
    void set_actor_config(UTestTubeSpinnersConfig* const new_config) noexcept {
        actor_config = new_config;
    }

    // ITestEntity
    auto get_entity_handle() const noexcept -> FRegistryEntityHandle override {
        return entity_handle;
    }
    void set_entity_handle(FRegistryEntityHandle const handle) noexcept { entity_handle = handle; }
#if WITH_EDITOR
    void set_test_name(FName const new_test_name) noexcept { test_name = new_test_name; }
    auto get_test_name() const noexcept -> FName override { return test_name; }
#endif
  private:
#if WITH_EDITOR
    UFUNCTION(CallInEditor, Category = "Proxy|Add Points")
    void add_fire_point();
    UFUNCTION(CallInEditor, Category = "Proxy|Remove Points")
    void remove_all_fire_points();
    UFUNCTION(CallInEditor, Category = "Proxy|Remove Points")
    void remove_fire_point();

    UFUNCTION(CallInEditor, Category = "Proxy|Save Asset")
    void save_configuration_to_asset();
    UFUNCTION(CallInEditor, Category = "Proxy|Load Asset")
    void apply_asset_configuration();
    UFUNCTION(CallInEditor, Category = "Proxy|Load Asset")
    void apply_asset_configuration_to_all_instances();

    UFUNCTION(CallInEditor, Category = "Proxy|Position Points")
    void position_fire_points();
    UFUNCTION(CallInEditor, Category = "Proxy|Position Points")
    void set_random_active_fire_point();
    UFUNCTION(CallInEditor, Category = "Proxy|Position Points")
    void set_random_active_fire_point_to_all_instances();
#endif
    void add_fire_points(int32 const n);
    void remove_fire_points(int32 const n);
    void face_fire_points_away_from_mesh();

    UPROPERTY(EditAnywhere, Category = "Proxy")
    TObjectPtr<UTestTubeSpinnersConfig> actor_config{nullptr};

    UPROPERTY(EditAnywhere, Category = "Proxy")
    TObjectPtr<UStaticMeshComponent> mesh{nullptr};

    UPROPERTY(EditAnywhere, Category = "Proxy")
    TArray<TObjectPtr<UArrowComponent>> fire_points{};

    UPROPERTY(EditAnywhere, Category = "Proxy")
    int32 initial_active_fire_point{0};

#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere, Category = "Test")
    FName test_name{NAME_None};
#endif

    FRegistryEntityHandle entity_handle;
};
