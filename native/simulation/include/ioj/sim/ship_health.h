#pragma once

#include <cstdint>

namespace ioj::sim {
[[nodiscard]] auto clamp_health_to_max(std::int32_t health, std::int32_t max_health) noexcept
    -> std::int32_t;

struct ShipHealth {
    inline static constexpr std::int32_t default_max_health{100};
    inline static constexpr std::int32_t upgraded_max_health{150};

    ShipHealth() = default;
    ShipHealth(std::int32_t const health_value, std::int32_t const maximum)
        : health{health_value}
        , max_health{maximum} {}
    explicit ShipHealth(std::int32_t const maximum)
        : health{maximum}
        , max_health{maximum} {}

    auto operator==(ShipHealth const&) const noexcept -> bool = default;
    void upgrade_max_health() { max_health = upgraded_max_health; }
    auto is_alive() const noexcept -> bool { return health > 0; }
    void clamp_to_max() noexcept { health = clamp_health_to_max(health, max_health); }

    std::int32_t health{default_max_health};
    std::int32_t max_health{default_max_health};
};
} // namespace ioj::sim
