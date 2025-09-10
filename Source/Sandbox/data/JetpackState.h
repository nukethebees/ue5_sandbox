#pragma once

#include "JetpackState.generated.h"

USTRUCT(BlueprintType)
struct FJetpackState {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float fuel_remaining{0.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float fuel_capacity{0.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool is_jetpacking{false};

    FJetpackState() = default;
    FJetpackState(float fuel_remaining, float fuel_capacity, bool is_jetpacking)
        : fuel_remaining(fuel_remaining)
        , fuel_capacity(fuel_capacity)
        , is_jetpacking(is_jetpacking) {}
};
