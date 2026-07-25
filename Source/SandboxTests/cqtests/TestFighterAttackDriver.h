#pragma once

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

    auto get_setup_error() const { return setup_error; }
    auto get_fight_duration() const { return fight_duration; }
  protected:
    void PostInitializeComponents() override;
    void BeginPlay() override;

    // Hero
    UPROPERTY(EditAnywhere, Category = "Test")
    TObjectPtr<ATestCapitalShipProxy> hero{nullptr};

    ETestTeam hero_team;

    // Enemy
    UPROPERTY(EditAnywhere, Category = "Test")
    TObjectPtr<ATestCapitalShipProxy> enemy{nullptr};

    ETestTeam enemy_team;

    // Test setup
    UPROPERTY(EditAnywhere, Category = "Test")
    float fight_duration{5.f};

    FString setup_error{};
};
