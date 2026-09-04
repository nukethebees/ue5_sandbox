#pragma once

#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/simulation/SimulationActorClasses.h>

#include <CoreMinimal.h>
#include <Engine/DataAsset.h>

#include "TestSimulationConfig.generated.h"

class USimulationConfig;
class ASpaceGamePlayerController;

UCLASS(BlueprintType)
class SPACEGAME_API UTestSimulationConfig : public UDataAsset {
    GENERATED_BODY()
  public:
    [[nodiscard]] auto is_valid() const noexcept -> bool;

    UPROPERTY(EditAnywhere, Category = "Simulation")
    TObjectPtr<USimulationConfig> simulation_config{nullptr};

    UPROPERTY(EditAnywhere, Category = "Simulation")
    TSubclassOf<ASpaceGamePlayerController> player_controller_class{nullptr};

    UPROPERTY(EditAnywhere, Category = "Simulation", meta = (ShowOnlyInnerProperties))
    FSimulationActorClasses actor_classes;
};
