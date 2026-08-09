#pragma once

#include "TestEntity.h"
#include "TestTeam.h"

#include <CoreMinimal.h>
#include <GameFramework/Actor.h>
#include <Misc/Optional.h>
#include <UObject/ScriptInterface.h>

#include "TestCapitalShipProxy.generated.h"

class UStaticMeshComponent;
class UArrowComponent;

class UTestCapitalShipsConfig;

UCLASS()
class ATestCapitalShipProxy
    : public AActor
    , public ITestEntity {
    GENERATED_BODY()
  public:
    ATestCapitalShipProxy();

    void set_actor_config(UTestCapitalShipsConfig* const new_config) noexcept {
        actor_config = new_config;
    }
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

    auto get_team() const noexcept { return team; }
    auto get_target_ship() const noexcept { return target_ship; }
    auto get_health() const noexcept { return health; }
    auto get_initial_spawn_delay() const noexcept { return initial_spawn_delay; }
    auto get_spawn_cooldown() const noexcept { return spawn_cooldown; }

    void set_health(TOptional<int32> const new_health) noexcept { health = new_health; }

    // ITestEntity
    auto get_entity_handle() const noexcept -> FRegistryEntityHandle override {
        return entity_handle;
    }
    void set_entity_handle(FRegistryEntityHandle const h) noexcept { entity_handle = h; }
#endif
  protected:
    UPROPERTY(EditAnywhere, Category = "Ship")
    TObjectPtr<UTestCapitalShipsConfig> actor_config{nullptr};

    UPROPERTY(EditAnywhere, Category = "Ship")
    TObjectPtr<UStaticMeshComponent> mesh{nullptr};

    UPROPERTY(EditAnywhere, Category = "Ship")
    TArray<TObjectPtr<UArrowComponent>> fighter_spawn_slots;

    UPROPERTY(EditAnywhere, Category = "Ship")
    TObjectPtr<AActor> target_ship{nullptr};

    UPROPERTY(EditAnywhere, Category = "Ship")
    ETestTeam team{ETestTeam::White};

    UPROPERTY(EditAnywhere, Category = "Ship")
    TOptional<int32> health{NullOpt};

    UPROPERTY(EditAnywhere, Category = "Ship")
    TOptional<float> initial_spawn_delay{NullOpt};

    UPROPERTY(EditAnywhere, Category = "Ship")
    TOptional<float> spawn_cooldown{NullOpt};

    FRegistryEntityHandle entity_handle;
};
