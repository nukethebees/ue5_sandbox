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
  protected:
    void BeginPlay() override;

    UPROPERTY(EditAnywhere, Category = "Test")
    TObjectPtr<ATestCapitalShipProxy> hero{nullptr};

    ETestTeam hero_team;

    UPROPERTY(EditAnywhere, Category = "Test")
    TObjectPtr<ATestCapitalShipProxy> enemy{nullptr};

    ETestTeam enemy_team;
};
