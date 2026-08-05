#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "SimulationConfig.generated.h"

class UTestSpaceShipData;
class UTestLasersConfig;
class UTestCapitalShipsConfig;
class UTestCapitalShipFightersConfig;
class UTestStaticTurretsConfig;
class UTestTubeSpinnersConfig;

UENUM(BlueprintType)
enum class ESimulationAssetActorScope : uint8 {
    OrchestratorActors,
    AllActorsInLevel,
};

UENUM(BlueprintType)
enum class ESimulationAssetProxyMode : uint8 {
    Exclude,
    Include,
};

UCLASS(BlueprintType)
class SANDBOX_API USimulationConfig : public UDataAsset {
    GENERATED_BODY()
  public:
    [[nodiscard]] auto is_valid() const noexcept -> bool;

    UPROPERTY(EditAnywhere, Category = "Simulation")
    TObjectPtr<UTestSpaceShipData> player_ship_config{nullptr};

    UPROPERTY(EditAnywhere, Category = "Simulation")
    TObjectPtr<UTestLasersConfig> lasers_config{nullptr};

    UPROPERTY(EditAnywhere, Category = "Simulation")
    TObjectPtr<UTestCapitalShipsConfig> capital_ships_config{nullptr};

    UPROPERTY(EditAnywhere, Category = "Simulation")
    TObjectPtr<UTestCapitalShipFightersConfig> capital_ship_fighters_config{nullptr};

    UPROPERTY(EditAnywhere, Category = "Simulation")
    TObjectPtr<UTestStaticTurretsConfig> static_turrets_config{nullptr};

    UPROPERTY(EditAnywhere, Category = "Simulation")
    TObjectPtr<UTestTubeSpinnersConfig> tube_spinners_config{nullptr};
};
