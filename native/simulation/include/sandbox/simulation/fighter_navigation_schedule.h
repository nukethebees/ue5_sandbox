#pragma once

#include "sandbox/simulation/index_span.h"

#include <cstdint>
#include <span>

namespace ml::simulation::fighters {
struct NavigationScratch;

void collect_navigation_updates(std::span<FIndexSpan const> task_spans,
                                std::span<std::int16_t> remaining_ticks,
                                std::span<std::int16_t const> periods,
                                NavigationScratch& scratch);
}
