#pragma once
#include <algorithm>
#include <cstdint>
#include <ioj/sim/sim_clock.h>
#include <ioj/sim/system_read_views.h>
#include <optional>

#include <ioj/sim/player/player_read_view.h>

namespace ioj::sim {
struct MissionManager;

// Borrowed buffers remain valid until the next advance or authoritative state mutation.
struct LevelReadView {
    std::uint64_t frame_sequence{};
    SimClock const* clock{};
    CapitalReadView capitals;
    FighterReadView fighters;
    TurretReadView turrets;
    SpinnerReadView spinners;
    LaserReadView lasers;
    std::optional<PlayerReadView> player;
    MissionManager const* mission{};
    auto interpolation_alpha() const -> double {
        return std::clamp(clock->tick_loop.accumulator / clock->get_tick_period(), 0.0, 1.0);
    }
};
} // namespace ioj::sim
