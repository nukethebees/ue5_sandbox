#pragma once

#include "ioj/sim/index_span.h"
#include "sandbox/core/periodic_tick_countdown.h"

#include <cstdint>
#include <span>

namespace ioj::sim::fighters {
struct NavigationScratch;

void collect_navigation_updates(std::span<IndexSpan const> task_spans,
                                ml::PeriodicTickCountdownView<std::int16_t> countdowns,
                                NavigationScratch& scratch);
}
