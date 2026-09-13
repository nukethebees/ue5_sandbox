#pragma once

#include "sandbox/core/periodic_tick_countdown.h"
#include "sandbox/simulation/index_span.h"

#include <cstdint>
#include <span>

namespace ml::simulation::fighters {
struct NavigationScratch;

void collect_navigation_updates(std::span<FIndexSpan const> task_spans,
                                PeriodicTickCountdownView<std::int16_t> countdowns,
                                NavigationScratch& scratch);
}
