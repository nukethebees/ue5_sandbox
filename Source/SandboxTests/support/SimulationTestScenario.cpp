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

void FSimulationTestScenario::set_timeline_end_tick_hook(FOrchestratorEndTickTestHook sample_hook) {
    context_.orchestrator.set_end_tick_test_hook(FOrchestratorEndTickTestHook::CreateLambda(
        [this, sample_hook = MoveTemp(sample_hook)](ATestBatchOrchestrator& orchestrator) mutable {
            sample_hook.ExecuteIfBound(orchestrator);
            test_driver->advance_timeline();
        }));
}
}
