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
    CONTINUOUS_END UMETA(Hidden),
};

constexpr bool is_discrete(EAmmoType type) {
    constexpr auto end{std::to_underlying(EAmmoType::DISCRETE_END)};

    return std::to_underlying(type) < end;
}

constexpr bool is_continuous(EAmmoType type) {
    return !is_discrete(type);
}

constexpr int32 num_discrete_types(EAmmoType type) {
    return static_cast<int32>(EAmmoType::DISCRETE_END);
}
constexpr int32 num_continuous_types(EAmmoType type) {
    constexpr auto d_end{std::to_underlying(EAmmoType::DISCRETE_END)};
    constexpr auto c_end{std::to_underlying(EAmmoType::CONTINUOUS_END)};
    constexpr auto diff{c_end - d_end};
    return diff - 1;
}
