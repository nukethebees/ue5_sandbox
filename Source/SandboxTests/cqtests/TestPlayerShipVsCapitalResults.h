#pragma once

#include <CoreMinimal.h>
#include <Engine/DataTable.h>

#include "TestPlayerShipVsCapitalResults.generated.h"

USTRUCT()
struct FPlayerShipVsCapitalResultRow : public FTableRowBase {
    GENERATED_BODY()

    UPROPERTY()
    double time{0.0};

    UPROPERTY()
    uint64 tick{0};

    UPROPERTY()
    FVector player_ship_location{FVector::ZeroVector};

    UPROPERTY()
    FVector player_ship_registry_location{FVector::ZeroVector};

    UPROPERTY()
    TArray<FVector> fighter_target_locations;

    UPROPERTY()
    TArray<FVector> fighter_locations;
};
