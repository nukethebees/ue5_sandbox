#pragma once

#include <SpaceGame/simulation/TestBatchOrchestrator.h>

namespace ioj {
template <typename Observe>
void advance_simulation(ATestBatchOrchestrator& orchestrator, double const dt, Observe&& observe) {
    orchestrator.SetActorTickEnabled(false);
    auto const* const level{orchestrator.get_level_simulation()};
    check(level);
    auto const previous_tick{level->clock().get_completed_ticks()};
    orchestrator.tick(dt);
    auto const* const current{orchestrator.get_level_simulation()};
    if (current && current->clock().get_completed_ticks() != previous_tick) {
        observe(orchestrator);
    }
    orchestrator.SetActorTickEnabled(false);
}
}
