#pragma once

#include "CoreMinimal.h"

#include "Sandbox/combat/weapons/enums/AmmoType.h"

#include "AmmoData.generated.h"

USTRUCT(BlueprintType)
struct FAmmoData {
    GENERATED_BODY()

    FAmmoData() = default;
    FAmmoData(EAmmoType type)
        : type(type) {}
    FAmmoData(EAmmoType type, int32 amount)
        : type(type)
        , amount(amount) {}

    bool is_empty() const { return ml::is_discrete(type) && (amount == 0); }

    auto& operator+=(FAmmoData const& other) {
        if (type == other.type) {
            amount += other.amount;
        }

        return *this;
    }
    auto& operator-=(FAmmoData const& other) {
        if (type == other.type) {
            amount -= other.amount;
        }

        return *this;
    }

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EAmmoType type{EAmmoType::Bullets};

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 amount{0};
};
