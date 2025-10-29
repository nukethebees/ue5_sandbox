#pragma once

#include "CoreMinimal.h"

#include "Sandbox/combat/weapons/enums/AmmoType.h"

#include "AmmoData.generated.h"

USTRUCT(BlueprintType)
struct FAmmoData {
    GENERATED_BODY()

    FAmmoData() = default;
    FAmmoData(EAmmoType type, int32 amount)
        : type(type)
        , discrete_amount(amount) {}
    FAmmoData(EAmmoType type, float amount)
        : type(type)
        , continuous_amount(amount) {}

    static FAmmoData make_discrete(EAmmoType type, int32 amount) {
        FAmmoData data;
        data.type = type;
        data.discrete_amount = amount;
        return data;
    }

    static FAmmoData make_continuous(EAmmoType type, float amount) {
        FAmmoData data;
        data.type = type;
        data.continuous_amount = amount;
        return data;
    }

    bool is_empty(FAmmoData const& data) {
        return is_discrete(data.type) ? data.discrete_amount == 0 : data.continuous_amount <= 0.f;
    }

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EAmmoType type{EAmmoType::Bullets};

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 discrete_amount{0};

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float continuous_amount{0.f};
};
