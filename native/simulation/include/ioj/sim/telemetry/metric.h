#pragma once

#include <ioj/enum_traits.h>

#include <cstdint>
#include <string_view>

namespace ioj::sim::telemetry {
enum class Metric : std::uint8_t {
    ActiveEntities,
    PlayerShips,
    Turrets,
    CapitalShips,
    CapitalShipFighters,
    TubeSpinners,
    ActiveLasers,
    SpawnRate,
    DestructionRate,
    KillRate,
    LaserFireRate,
    Count,
};

[[nodiscard]] auto metric_title(Metric metric) noexcept -> std::string_view;
[[nodiscard]] auto metric_units(Metric metric) noexcept -> std::string_view;
}

namespace ioj {
template <>
[[nodiscard]] constexpr auto enum_count<sim::telemetry::Metric>() noexcept -> std::size_t {
    return static_cast<std::size_t>(sim::telemetry::Metric::Count);
}
}
