#pragma once

#include <CoreMinimal.h>
#include <Engine/DataAsset.h>

#include "TestFightersInterceptCapitalResults.generated.h"

USTRUCT()
struct FFightersInterceptCapitalTimeSeriesRow {
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere)
    double time{0.0};

    UPROPERTY(VisibleAnywhere)
    FString parent_target;

    UPROPERTY(VisibleAnywhere)
    TArray<FString> fighter_targets;
};

UCLASS()
class UTestFightersInterceptCapitalResults : public UDataAsset {
    GENERATED_BODY()
  public:
    UPROPERTY(VisibleAnywhere)
    FString hero_capital;

    UPROPERTY(VisibleAnywhere)
    FString original_target;

    UPROPERTY(VisibleAnywhere)
    FString intercept_target;

    UPROPERTY(VisibleAnywhere)
    TArray<FFightersInterceptCapitalTimeSeriesRow> time_series_results;
};
