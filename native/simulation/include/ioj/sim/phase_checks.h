#pragma once

#include <ioj/sim/sim_clock.h>

namespace ioj::sim {
[[nodiscard]] inline auto is_preparation_mutation_allowed(SimClock const& clock) noexcept -> bool {
    auto const phase{clock.phase()};
    auto const state{clock.get_level_state()};
    return state == LevelState::Uninitialised ||
           (state == LevelState::Running && phase == SimulationPhase::Preparation);
}

[[nodiscard]] inline auto is_removal_allowed(SimClock const& clock) noexcept -> bool {
    return clock.get_level_state() == LevelState::Running &&
           clock.phase() == SimulationPhase::ResolutionCommit;
}

[[nodiscard]] inline auto is_structural_mutation_allowed(SimClock const& clock) noexcept -> bool {
    return is_preparation_mutation_allowed(clock) || is_removal_allowed(clock);
}

[[nodiscard]] inline auto is_lookup_allowed(SimulationPhase const phase,
                                            LevelState const state) noexcept -> bool {
    return state == LevelState::StableSetup ||
           (state == LevelState::Running && phase >= SimulationPhase::Thinking &&
            phase <= SimulationPhase::Resolution);
}

[[nodiscard]] inline auto is_lookup_allowed(SimClock const& clock) noexcept -> bool {
    return is_lookup_allowed(clock.phase(), clock.get_level_state());
}
}
