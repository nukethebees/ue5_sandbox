#pragma once

#include <ioj/sim/entity_unique_id.h>

#include <sandbox/core/enum_array.h>

#include <cstddef>
#include <cstdint>

namespace ioj::sim {
using EntityTypeSizes =
    ml::EnumArray<EntityType, std::uint32_t, static_cast<std::size_t>(EntityType::COUNT)>;

inline constexpr EntityTypeSizes entity_lifetime_capacities{[] {
    EntityTypeSizes capacities;
    capacities[EntityType::PlayerShip] = 1;
    capacities[EntityType::CapitalShip] = 20'000;
    capacities[EntityType::Turret] = 50'000;
    capacities[EntityType::Fighter] = 500'000;
    capacities[EntityType::TubeSpinner] = 1'000;
    return capacities;
}()};

inline constexpr std::uint32_t entity_identity_capacity{[] {
    std::uint32_t count{};
    auto const entity_type_count{EntityTypeSizes::size()};
    for (std::size_t i{}; i < entity_type_count; ++i) {
        count += entity_lifetime_capacities[static_cast<EntityType>(i)];
    }
    return count;
}()};
static_assert(entity_identity_capacity <= EntityUniqueId::index_field::value_mask);

[[nodiscard]] inline constexpr auto is_entity_identity_offset(EntityUniqueId const id) noexcept
    -> bool {
    if (!id.is_valid()) {
        return false;
    }
    auto const type{id.entity_type()};
    auto const offset{id.index()};
    return offset < entity_lifetime_capacities[type];
}
}
