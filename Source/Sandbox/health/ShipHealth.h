// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "ShipHealth.generated.h"

USTRUCT(BlueprintType)
struct SANDBOX_API FShipHealth {
    GENERATED_BODY()

    FShipHealth() = default;
    FShipHealth(int32 health, int32 max_health)
        : health(health)
        , max_health(max_health) {}

    UPROPERTY(EditAnywhere, Category = "Health")
    int32 health{100};
    UPROPERTY(EditAnywhere, Category = "Health")
    int32 max_health{100};
};
