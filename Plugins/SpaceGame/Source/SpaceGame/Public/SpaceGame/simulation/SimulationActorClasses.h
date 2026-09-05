#pragma once

#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>

#include "CoreMinimal.h"

#include "SimulationActorClasses.generated.h"

USTRUCT()
struct SPACEGAME_API FSimulationActorClasses {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Simulation")
    TSubclassOf<ATestSpaceShip> player_ship_class{nullptr};

    UPROPERTY(EditAnywhere, Category = "Simulation")
    TSubclassOf<ATestCapitalShipProxy> capital_ship_proxy_class{nullptr};
};
