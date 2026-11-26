#pragma once
#include <ioj/kill_count.h>
#include <ioj/sim/entities/ship_health.h>
#include <ioj/sim/player/trigger_mode.h>

#include <cstdint>

namespace ioj::presentation {
struct PlayerStatusHudData {
    auto operator==(PlayerStatusHudData const&) const -> bool = default;

    sim::ShipHealth health{};
    float speed{};
    float target_speed{};
    float energy{1.f};
    KillCount points{};
    sim::ShipTriggerMode trigger_mode{sim::ShipTriggerMode::Single};
};
}
