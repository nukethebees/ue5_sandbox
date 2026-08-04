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
    FVector player_ship_location{FVector::ZeroVector};

    UPROPERTY()
    FVector player_ship_registry_location{FVector::ZeroVector};

    UPROPERTY()
    int32 fighter_index{-1};

    UPROPERTY()
    bool has_fighter_target_location{false};

    UPROPERTY()
    FVector fighter_target_location{FVector::ZeroVector};

    UPROPERTY()
    bool has_fighter_location{false};

    UPROPERTY()
    FVector fighter_location{FVector::ZeroVector};
};
