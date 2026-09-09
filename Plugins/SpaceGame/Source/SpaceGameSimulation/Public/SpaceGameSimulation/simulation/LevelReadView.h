#pragma once
#include <SpaceGameSimulation/simulation/SimulationClock.h>
#include <SpaceGameSimulation/simulation/SystemReadViews.h>

#include <SpaceGameSimulation/ships/player/PlayerReadView.h>
struct FTestMissionManager;

// Borrowed buffers remain valid until the next advance or authoritative state mutation.
struct FLevelReadView {
    uint64 frame_sequence{};
    FSimulationClock const* clock{};
    FCapitalReadView capitals;
    FFighterReadView fighters;
    FTurretReadView turrets;
    FSpinnerReadView spinners;
    FLaserReadView lasers;
    TOptional<FPlayerReadView> player;
    FTestEntityRegistry const* registry{};
    FTestMissionManager const* mission{};
    auto interpolation_alpha() const -> double {
        return FMath::Clamp(clock->tick_loop.accumulator / clock->get_tick_period(), 0.0, 1.0);
    }
};
