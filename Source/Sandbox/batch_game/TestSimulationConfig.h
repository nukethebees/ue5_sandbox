#pragma once

#include <Sandbox/batch_game/SimulationActorClasses.h>

#include <CoreMinimal.h>
#include <Engine/DataAsset.h>

#include "TestSimulationConfig.generated.h"

class USimulationConfig;

UCLASS(BlueprintType)
class SANDBOX_API UTestSimulationConfig : public UDataAsset {
    GENERATED_BODY()
  public:
    [[nodiscard]] auto is_valid() const noexcept -> bool;

    UPROPERTY(EditAnywhere, Category = "Simulation")
    TObjectPtr<USimulationConfig> simulation_config{nullptr};

    UPROPERTY(EditAnywhere, Category = "Simulation", meta = (ShowOnlyInnerProperties))
    FSimulationActorClasses actor_classes;
};
