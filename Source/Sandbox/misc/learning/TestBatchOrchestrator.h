#pragma once

#include <CoreMinimal.h>
#include <GameFramework/Actor.h>

#include "TestBatchOrchestrator.generated.h"

class ATestSpaceShip;
class ATestLasers;
class ATestCapitalShips;
class ATestCapitalShipFighters;
class ATestStaticTurrets;
class ATestTubeSpinners;
class ATestEntityRegistry;

UCLASS()
class ATestBatchOrchestrator : public AActor {
    GENERATED_BODY()
  public:
    ATestBatchOrchestrator();

    void Tick(float dt) override;
    void tick(float const dt);
  protected:
    void BeginPlay() override;
  private:
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestSpaceShip> player_ship{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestLasers> lasers{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestCapitalShips> capital_ships{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestCapitalShipFighters> capital_ship_fighters{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestStaticTurrets> turrets{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestTubeSpinners> spinners{nullptr};

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestEntityRegistry> entity_registry{nullptr};

    UPROPERTY(Transient)
    uint64 tick_counter{0};
};
