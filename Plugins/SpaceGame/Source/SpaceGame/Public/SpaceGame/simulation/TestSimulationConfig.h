#pragma once

#include <SpaceGame/simulation/SimulationActorClasses.h>
#include <SpaceGame/ships/player/TestSpaceShipController.h>

#include <CoreMinimal.h>
#include <Engine/DataAsset.h>

#include "TestSimulationConfig.generated.h"

class USimulationConfig;
class ATestSpaceShipController;

UCLASS(BlueprintType)
class SPACEGAME_API UTestSimulationConfig : public UDataAsset {
    GENERATED_BODY()
  public:
    [[nodiscard]] auto is_valid() const noexcept -> bool;

    UPROPERTY(EditAnywhere, Category = "Simulation")
    TObjectPtr<USimulationConfig> simulation_config{nullptr};

    UPROPERTY(EditAnywhere, Category = "Simulation")
    TSubclassOf<ATestSpaceShipController> player_controller_class{nullptr};

    UPROPERTY(EditAnywhere, Category = "Simulation", meta = (ShowOnlyInnerProperties))
    FSimulationActorClasses actor_classes;
};
