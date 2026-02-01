#pragma once

#include <array>
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

namespace ml {
inline static constexpr std::array<EAmmoType, 6> ammo_types{
    EAmmoType::Bullets,
    EAmmoType::Shells,
    EAmmoType::Rockets,
    EAmmoType::Grenades,
    EAmmoType::Energy,
    EAmmoType::Plasma,
};

constexpr bool is_discrete(EAmmoType type) {
    constexpr auto end{std::to_underlying(EAmmoType::DISCRETE_END)};

    return std::to_underlying(type) < end;
}

constexpr bool is_continuous(EAmmoType type) {
    return !is_discrete(type);
}

constexpr int32 ammo_type_discrete_begin() {
    return 0;
}
constexpr int32 ammo_type_discrete_end() {
    return static_cast<int32>(EAmmoType::DISCRETE_END);
}
constexpr int32 ammo_type_continuous_begin() {
    return static_cast<int32>(EAmmoType::DISCRETE_END) + 1;
}
constexpr int32 ammo_type_continuous_end() {
    return static_cast<int32>(EAmmoType::CONTINUOUS_END);
}

constexpr int32 num_ammo_discrete_types() {
    return static_cast<int32>(EAmmoType::DISCRETE_END);
}
constexpr int32 num_ammo_continuous_types() {
    constexpr auto d_end{std::to_underlying(EAmmoType::DISCRETE_END)};
    constexpr auto c_end{std::to_underlying(EAmmoType::CONTINUOUS_END)};
    constexpr auto diff{c_end - d_end};
    return diff - 1;
}
constexpr int32 num_ammo_types() {
    return static_cast<int32>(EAmmoType::CONTINUOUS_END) - 2;
}

}
