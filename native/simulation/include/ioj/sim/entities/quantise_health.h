#pragma once

#include <ioj/sim/entities/entity_instance_handle.h>
#include <ioj/sim/entities/health_type.h>

#include <algorithm>
#include <cassert>
#include <cstdint>

namespace ioj::sim {
// Require positive health and maximum; clamp overheal to the highest band.
[[nodiscard]] inline auto quantise_health(Health const health, Health const maximum) noexcept
    -> QuantisedHealth::value_type {
    assert(health.value() > 0 && maximum.value() > 0);
    // Keep exact quarter boundaries in the lower band and clamp overheal.
    auto const band{(static_cast<std::int64_t>(health.value()) * 4 - 1) / maximum.value()};
    return static_cast<QuantisedHealth::value_type>(std::min<std::int64_t>(band, 3));
}
}
