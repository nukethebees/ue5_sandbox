#pragma once

#include <SpaceGame/ships/fighters/TestCapitalShipFighters.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/capital/TestCapitalShips.h>
#include <SpaceGame/combat/lasers/TestLasers.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/defences/turrets/TestStaticTurrets.h>
#include <SpaceGame/defences/spinners/TestTubeSpinners.h>
#include <SpaceGame/effects/DelayedNiagaraSpawner.h>

#include "CoreMinimal.h"

#include "SimulationActorClasses.generated.h"

USTRUCT()
struct SPACEGAME_API FSimulationActorClasses {
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
