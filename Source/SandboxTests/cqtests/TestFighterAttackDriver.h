#pragma once

#include <Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h>
#include <Sandbox/batch_game/TestTeam.h>

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestFighterAttackDriver.generated.h"

class UInstancedStaticMeshComponent;

class ATestCapitalShipProxy;

UCLASS()
class ATestFighterAttackDriver : public AActor {
    GENERATED_BODY()
  public:
    ATestFighterAttackDriver();

    auto get_hero_team() const { return hero_team; }
    auto get_enemy_team() const { return enemy_team; }
    auto get_hero_handle() const { return hero_handle; }
    auto get_enemy_handle() const { return enemy_handle; }

    auto get_setup_error() const { return setup_error; }
    auto get_fight_duration() const { return fight_duration; }
  protected:
    void PostInitializeComponents() override;
    void BeginPlay() override;
    void bind_proxy_handles(TMap<AActor const*, FRegistryEntityHandle> const& proxy_handles);

    // Hero
    UPROPERTY(EditAnywhere, Category = "Test")
    TObjectPtr<ATestCapitalShipProxy> hero{nullptr};

    ETestTeam hero_team;
    FRegistryEntityHandle hero_handle;

    // Enemy
    UPROPERTY(EditAnywhere, Category = "Test")
    TObjectPtr<ATestCapitalShipProxy> enemy{nullptr};

    ETestTeam enemy_team;
    FRegistryEntityHandle enemy_handle;

    // Test setup
    UPROPERTY(EditAnywhere, Category = "Test")
    float fight_duration{5.f};

    FString setup_error{};
};
