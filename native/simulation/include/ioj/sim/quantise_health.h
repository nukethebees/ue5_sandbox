#pragma once

#include <ioj/sim/entity_instance_handle.h>
#include <ioj/sim/health_type.h>

#include <algorithm>
#include <cassert>
#include <cstdint>

namespace ioj::sim {
// Require positive health and maximum; clamp overheal to the highest band.
[[nodiscard]] inline auto quantise_health(Health const health, Health const maximum) noexcept
    -> QuantisedHealth {
    assert(health > 0 && maximum > 0);
    // Keep exact quarter boundaries in the lower band and clamp overheal.
    auto const band{(static_cast<std::int64_t>(health) * 4 - 1) / maximum};
    return static_cast<QuantisedHealth>(std::min<std::int64_t>(band, 3));
}
}
