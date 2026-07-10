#pragma once

#include "TestEntity.h"
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
class ATestTubeSpinnerProxy
    : public AActor
    , public ITestEntity {
  public:
    GENERATED_BODY()

    ATestTubeSpinnerProxy();

    auto get_initial_active_fire_point() const { return initial_active_fire_point; }

    // ITestEntity
    auto get_entity_handle() const noexcept -> FRegistryEntityHandle override {
        return entity_handle;
    }
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

    FRegistryEntityHandle entity_handle;
};
