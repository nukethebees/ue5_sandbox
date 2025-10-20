#pragma once

#include <utility>

#include "CoreMinimal.h"

#include "AmmoType.generated.h"

UENUM(BlueprintType)
enum class EAmmoType : uint8 {
    // Discrete ammo types (countable)
    Bullets UMETA(DisplayName = "Bullets"),
    Shells UMETA(DisplayName = "Shells"),
    Rockets UMETA(DisplayName = "Rockets"),
    Grenades UMETA(DisplayName = "Grenades"),
    DISCRETE_END UMETA(Hidden),

    // Continuous ammo types (measurable)
    Energy UMETA(DisplayName = "Energy"),
    Plasma UMETA(DisplayName = "Plasma"),
};

constexpr bool is_discrete(EAmmoType type) {
    constexpr auto end{std::to_underlying(EAmmoType::DISCRETE_END)};

    return std::to_underlying(type) < end;
}

constexpr bool is_continuous(EAmmoType type) {
    return !is_discrete(type);
}
