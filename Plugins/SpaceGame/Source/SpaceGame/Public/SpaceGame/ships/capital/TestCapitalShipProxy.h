#pragma once

#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include "SpaceGame/entities/TestEntity.h"
#include "SpaceGame/entities/TestTeam.h"

#include <CoreMinimal.h>
#include <GameFramework/Actor.h>
#include <Misc/Optional.h>
#include <UObject/ScriptInterface.h>

#include "TestCapitalShipProxy.generated.h"

class UStaticMeshComponent;
class UArrowComponent;

UCLASS()
class ATestCapitalShipProxy
    : public AActor
    , public ITestEntity {
    GENERATED_BODY()
  public:
    ATestCapitalShipProxy();

    void set_actor_config(FCapitalShipConfig const* const new_config) noexcept {
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
    auto get_team() const noexcept { return team; }
    auto get_target_ship() const noexcept { return target_ship; }
    auto get_health() const noexcept { return health; }
    auto get_initial_spawn_delay() const noexcept { return initial_spawn_delay; }
    auto get_spawn_cooldown() const noexcept { return spawn_cooldown; }

    void set_team(ETestTeam const new_team) noexcept { team = new_team; }
    void set_target_ship(AActor* const new_target_ship) noexcept { target_ship = new_target_ship; }
    void set_health(TOptional<int32> const new_health) noexcept { health = new_health; }
    void set_initial_spawn_delay(TOptional<float> const new_delay) noexcept {
        initial_spawn_delay = new_delay;
    }
    void set_spawn_cooldown(TOptional<float> const new_cooldown) noexcept {
        spawn_cooldown = new_cooldown;
    }

    void set_test_name(FName const new_test_name) noexcept { test_name = new_test_name; }

    // ITestEntity
    auto get_entity_handle() const noexcept -> FRegistryEntityHandle override {
        return entity_handle;
    }
    auto get_test_name() const noexcept -> FName override { return test_name; }
    void set_entity_handle(FRegistryEntityHandle const h) noexcept { entity_handle = h; }
#endif
  protected:
    FCapitalShipConfig const* actor_config{nullptr};

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

#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere, Category = "Test")
    FName test_name{NAME_None};
#endif

    FRegistryEntityHandle entity_handle;
};
