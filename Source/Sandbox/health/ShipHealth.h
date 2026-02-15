// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "ShipHealth.generated.h"

USTRUCT(BlueprintType)
struct SANDBOX_API FShipHealth {
    GENERATED_BODY()

    inline static constexpr int32 default_max_health{100};
    inline static constexpr int32 upgraded_max_health{150};

    FShipHealth() = default;
    FShipHealth(int32 health, int32 max_health)
        : health(health)
        , max_health(max_health) {}

    void upgrade_max_health() { max_health = upgraded_max_health; }

    UPROPERTY(EditAnywhere, Category = "Health")
    int32 health{100};
    UPROPERTY(EditAnywhere, Category = "Health")
    int32 max_health{default_max_health};
};
