#pragma once

#include <ioj/sim/entities/ship_health.h>
#include <ioj/sim/player/fire_rate.h>

#include <cstdint>

namespace ioj::presentation {
struct PlayerStatusHudData {
    auto operator==(PlayerStatusHudData const&) const -> bool = default;

    sim::ShipHealth health{};
    float speed{};
    float target_speed{};
    float energy{1.f};
    std::int32_t points{};
    sim::ShipFireRate fire_rate{sim::ShipFireRate::Single};
};
}
