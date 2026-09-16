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

inline constexpr EntityTypeSizes entity_identity_offsets{[] {
    EntityTypeSizes offsets;
    std::uint32_t offset{};
    for (std::size_t i{}; i < EntityTypeSizes::size(); ++i) {
        auto const type{static_cast<EntityType>(i)};
        offsets[type] = offset;
        offset += entity_lifetime_capacities[type];
    }
    return offsets;
}()};

[[nodiscard]] inline constexpr auto entity_identity_offset(EntityType const type,
                                                           std::uint32_t const ordinal) noexcept
    -> std::uint32_t {
    return entity_identity_offsets[type] + ordinal;
}

inline constexpr std::uint32_t entity_identity_capacity{[] {
    std::uint32_t count{};
    for (std::size_t i{}; i < EntityTypeSizes::size(); ++i) {
        count += entity_lifetime_capacities[static_cast<EntityType>(i)];
    }
    return count;
}()};
static_assert(entity_identity_capacity <= EntityUniqueId::index_value_mask);

[[nodiscard]] inline constexpr auto is_entity_identity_offset(EntityUniqueId const id) noexcept
    -> bool {
    if (!id.is_valid()) {
        return false;
    }
    auto const type{id.entity_type()};
    auto const offset{id.index()};
    auto const base{entity_identity_offsets[type]};
    return offset >= base && offset - base < entity_lifetime_capacities[type];
}
}
