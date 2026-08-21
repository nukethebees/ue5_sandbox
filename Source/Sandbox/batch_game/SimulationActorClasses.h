#pragma once

#include <Sandbox/batch_game/TestCapitalShipFighters.h>
#include <Sandbox/batch_game/TestCapitalShipProxy.h>
#include <Sandbox/batch_game/TestCapitalShips.h>
#include <Sandbox/batch_game/TestLasers.h>
#include <Sandbox/batch_game/TestSpaceShip.h>
#include <Sandbox/batch_game/TestStaticTurrets.h>
#include <Sandbox/batch_game/TestTubeSpinners.h>
#include <Sandbox/environment/effects/DelayedNiagaraSpawner.h>

#include "CoreMinimal.h"

#include "SimulationActorClasses.generated.h"

USTRUCT()
struct SANDBOX_API FSimulationActorClasses {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Simulation")
    TSubclassOf<ATestSpaceShip> player_ship_class{nullptr};

    UPROPERTY(EditAnywhere, Category = "Simulation")
    TSubclassOf<ATestLasers> lasers_class{nullptr};

    UPROPERTY(EditAnywhere, Category = "Simulation")
    TSubclassOf<ATestCapitalShips> capital_ships_class{nullptr};

    UPROPERTY(EditAnywhere, Category = "Simulation")
    TSubclassOf<ATestCapitalShipProxy> capital_ship_proxy_class{nullptr};

    UPROPERTY(EditAnywhere, Category = "Simulation")
    TSubclassOf<ATestCapitalShipFighters> capital_ship_fighters_class{nullptr};

    UPROPERTY(EditAnywhere, Category = "Simulation")
    TSubclassOf<ATestStaticTurrets> turrets_class{nullptr};

    UPROPERTY(EditAnywhere, Category = "Simulation")
    TSubclassOf<ATestTubeSpinners> spinners_class{nullptr};

    UPROPERTY(EditAnywhere, Category = "Simulation")
    TSubclassOf<ADelayedNiagaraSpawner> niagara_spawner_class{nullptr};
};
