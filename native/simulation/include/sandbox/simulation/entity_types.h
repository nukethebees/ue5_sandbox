#pragma once

#include <cstdint>
#include <limits>

namespace ml::simulation {
enum class Team : std::uint8_t {
    White,
    Red,
    Green,
    Blue,
    Orange,
    Yellow,
    COUNT,
};

enum class EntityType : std::uint8_t {
    PlayerShip,
    Turret,
    CapitalShip,
    CapitalShipFighter,
    TubeSpinner,
    COUNT,
};

struct EntityOwnerId {
    using ThisClass = EntityOwnerId;
    using value_type = std::uint8_t;

    static constexpr value_type NULL_ID{std::numeric_limits<value_type>::max()};

    [[nodiscard]] constexpr auto is_valid() const noexcept -> bool { return id != NULL_ID; }

    auto operator==(EntityOwnerId const&) const noexcept -> bool = default;

    value_type id{NULL_ID};
};

struct EntityUniqueId {
    using ThisClass = EntityUniqueId;
    using value_type = std::int32_t;

    static constexpr value_type NULL_ID{std::numeric_limits<value_type>::max()};

    [[nodiscard]] constexpr auto operator+(value_type const delta) const noexcept
        -> EntityUniqueId {
        return {.id = id + delta};
    }

    [[nodiscard]] constexpr auto is_valid() const noexcept -> bool { return id != NULL_ID; }

    auto operator==(EntityUniqueId const&) const noexcept -> bool = default;

    value_type id{NULL_ID};
};

enum class RegistryHandleState : std::uint8_t {
    Active,
    Stale,
    Invalid,
    Null,
};

enum class DeathReason : std::uint8_t {
    Unset,
    Unknown,
    Combat,
};
}
