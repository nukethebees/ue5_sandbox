#pragma once

#include <ioj/sim/entities/health.h>

#include <algorithm>

namespace ioj::sim {
struct ShipHealth {
    inline static constexpr Health default_max_health{100};
    inline static constexpr Health upgraded_max_health{150};

    ShipHealth() = default;
    ShipHealth(Health const health_value, Health const maximum)
        : health{health_value}
        , max_health{maximum} {}
    explicit ShipHealth(Health const maximum)
        : health{maximum}
        , max_health{maximum} {}

    auto operator==(ShipHealth const&) const noexcept -> bool = default;
    void upgrade_max_health() { max_health = upgraded_max_health; }
    auto is_alive() const noexcept -> bool { return sim::is_alive(health); }
    void clamp_to_max() noexcept { health = std::max(health, max_health); }

    Health health{default_max_health};
    Health max_health{default_max_health};
};
} // namespace ioj::sim
