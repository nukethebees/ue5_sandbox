#pragma once

#include <ioj/sim/sim_clock.h>

namespace ioj::sim {
[[nodiscard]] inline auto is_preparation_mutation_allowed(SimClock const& clock) noexcept -> bool {
    auto const phase{clock.phase()};
    return phase == SimulationPhase::Initialisation || phase == SimulationPhase::Preparation;
}

[[nodiscard]] inline auto is_removal_allowed(SimClock const& clock) noexcept -> bool {
    return clock.phase() == SimulationPhase::ResolutionCommit;
}

[[nodiscard]] inline auto is_structural_mutation_allowed(SimClock const& clock) noexcept -> bool {
    return is_preparation_mutation_allowed(clock) || is_removal_allowed(clock);
}

[[nodiscard]] inline auto is_lookup_allowed(SimClock const& clock) noexcept -> bool {
    // Keep lookup-valid phases below the generated boundary.
    return std::to_underlying(clock.phase()) < std::to_underlying(SimulationPhase::Initialisation);
}
}
