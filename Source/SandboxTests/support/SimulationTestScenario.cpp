#include "SimulationTestScenario.h"

#include "simulation_step.h"

namespace ioj {
void FSimulationTestScenario::tear_down() {
    sample_callback_.Unbind();
    on_tear_down();
}

auto FSimulationTestScenario::initialise_test_driver() -> TestSimulationDriver& {
    test_driver = TestSimulationDriver::from_world(context_.world);
    return *test_driver;
}

void FSimulationTestScenario::advance_sample() {
    if (!checks.all_passed || !test_driver.IsSet()) {
        return;
    }

    auto const& clock{context_.orchestrator.get_level_simulation()->get_clock()};
    auto const dt{clock.get_tick_period() / clock.get_time_scale().get()};
    advance_simulation(context_.orchestrator, dt, [this](ATestBatchOrchestrator& orchestrator) {
        sample_callback_.ExecuteIfBound(orchestrator);
        test_driver->advance_timeline();
    });
}

auto FSimulationTestScenario::advance_until_timeline_finished() -> bool {
    advance_sample();
    return !checks.all_passed || (test_driver.IsSet() && test_driver->timeline.is_finished());
}
}
