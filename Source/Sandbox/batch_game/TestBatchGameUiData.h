#pragma once

#include <CoreMinimal.h>
#include <Engine/DataAsset.h>

#include "TestBatchGameUiData.generated.h"

class UTestTeamVisualData;

USTRUCT(BlueprintType)
struct FTestBatchGameUiUpdateFrequencies {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "UI")
    float entity_count_update_period{0.25f};

    UPROPERTY(EditAnywhere, Category = "UI")
    float mission_status_update_period{0.25f};
};

UCLASS(BlueprintType)
class SANDBOX_API UTestBatchGameUiData : public UDataAsset {
    GENERATED_BODY()
  public:
    UPROPERTY(EditAnywhere, Category = "Update Frequencies")
    FTestBatchGameUiUpdateFrequencies update_frequencies{};

    UPROPERTY(EditAnywhere, Category = "Colours")
    TObjectPtr<UTestTeamVisualData> team_visual_data{nullptr};
};
