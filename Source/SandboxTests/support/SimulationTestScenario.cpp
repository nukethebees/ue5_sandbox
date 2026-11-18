#include "SimulationTestScenario.h"

#include "simulation_step.h"

namespace ioj {
void FSimulationTestScenario::tear_down() {
    on_tear_down();
}

auto FSimulationTestScenario::initialise_test_driver() -> TestSimulationDriver& {
    test_driver = TestSimulationDriver::from_world(context_.world);
    return *test_driver;
}

void FSimulationTestScenario::advance_tick() {
    if (!checks.all_passed || !test_driver.IsSet()) {
        return;
    }

    auto const& clock{context_.orchestrator.get_level_simulation()->clock()};
    auto const dt{clock.tick_period() / clock.time_scale().get()};
    context_.orchestrator.SetActorTickEnabled(false);
    context_.orchestrator.tick(dt);
    context_.orchestrator.SetActorTickEnabled(false);
}
}
