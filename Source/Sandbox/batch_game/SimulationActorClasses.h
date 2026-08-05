#pragma once

#include "CoreMinimal.h"

#include "SimulationActorClasses.generated.h"

class ATestSpaceShip;
class ATestLasers;
class ATestCapitalShips;
class ATestCapitalShipFighters;
class ATestStaticTurrets;
class ATestTubeSpinners;
class ATestEntityRegistry;
class ATestMissionManager;
class ADelayedNiagaraSpawner;

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
    TSubclassOf<ATestCapitalShipFighters> capital_ship_fighters_class{nullptr};

    UPROPERTY(EditAnywhere, Category = "Simulation")
    TSubclassOf<ATestStaticTurrets> turrets_class{nullptr};

    UPROPERTY(EditAnywhere, Category = "Simulation")
    TSubclassOf<ATestTubeSpinners> spinners_class{nullptr};

    UPROPERTY(EditAnywhere, Category = "Simulation")
    TSubclassOf<ATestEntityRegistry> entity_registry_class{nullptr};

    UPROPERTY(EditAnywhere, Category = "Simulation")
    TSubclassOf<ATestMissionManager> mission_manager_class{nullptr};

    UPROPERTY(EditAnywhere, Category = "Simulation")
    TSubclassOf<ADelayedNiagaraSpawner> niagara_spawner_class{nullptr};
};
