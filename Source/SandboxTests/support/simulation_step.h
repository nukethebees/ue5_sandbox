#pragma once

#include <SpaceGame/simulation/TestBatchOrchestrator.h>

namespace ioj {
template <typename Observe>
void advance_simulation(ATestBatchOrchestrator& orchestrator, double const dt, Observe&& observe) {
    orchestrator.SetActorTickEnabled(false);
    auto const previous_tick{orchestrator.get_completed_ticks()};
    orchestrator.tick(dt);
    if (orchestrator.get_completed_ticks() != previous_tick) {
        observe(orchestrator);
    }
    orchestrator.SetActorTickEnabled(false);
}
}
