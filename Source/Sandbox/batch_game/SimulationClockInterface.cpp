#include "SimulationClockInterface.h"

#include <Sandbox/batch_game/TestBatchOrchestrator.h>

namespace ml::test_batch_orchestrator {
void SimulationClockInterface::bind(ATestBatchOrchestrator const& new_orchestrator) noexcept {
    orchestrator = &new_orchestrator;
}

auto SimulationClockInterface::frequency_to_tick_period(time_type const frequency) const noexcept
    -> tick_type {
    check(IsValid(orchestrator));
    return orchestrator->frequency_to_tick_period(frequency);
}

auto SimulationClockInterface::duration_to_tick_period(time_type const duration) const noexcept
    -> tick_type {
    check(IsValid(orchestrator));
    return orchestrator->duration_to_tick_period(duration);
}

auto SimulationClockInterface::get_completed_ticks() const noexcept -> tick_type {
    check(IsValid(orchestrator));
    return orchestrator->get_completed_ticks();
}

auto SimulationClockInterface::get_simulation_time() const noexcept -> time_type {
    check(IsValid(orchestrator));
    return orchestrator->get_simulation_time();
}
auto SimulationClockInterface::get_tick_period() const noexcept -> time_type {
    check(IsValid(orchestrator));
    return orchestrator->get_tick_period();
}
}
