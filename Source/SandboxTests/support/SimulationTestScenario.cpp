#include "SimulationTestScenario.h"

#include <SpaceGame/simulation/TestBatchOrchestrator.h>

namespace ml {
void FSimulationTestScenario::tear_down() {
    context_.orchestrator.clear_end_tick_test_hook();
    on_tear_down();
}

auto FSimulationTestScenario::initialise_test_driver() -> TestSimulationDriver& {
    test_driver = TestSimulationDriver::from_world(context_.world);
    return *test_driver;
}
}
