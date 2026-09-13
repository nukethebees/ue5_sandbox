#pragma once
#include <algorithm>
#include <cstdint>
#include <optional>
#include <sandbox/simulation/simulation/SimulationClock.h>
#include <sandbox/simulation/simulation/SystemReadViews.h>

#include <sandbox/simulation/ships/player/PlayerReadView.h>
struct FTestMissionManager;

// Borrowed buffers remain valid until the next advance or authoritative state mutation.
struct FLevelReadView {
    std::uint64_t frame_sequence{};
    FSimulationClock const* clock{};
    FCapitalReadView capitals;
    FFighterReadView fighters;
    FTurretReadView turrets;
    FSpinnerReadView spinners;
    FLaserReadView lasers;
    std::optional<FPlayerReadView> player;
    FTestEntityRegistry const* registry{};
    FTestMissionManager const* mission{};
    auto interpolation_alpha() const -> double {
        return std::clamp(clock->tick_loop.accumulator / clock->get_tick_period(), 0.0, 1.0);
    }
};
