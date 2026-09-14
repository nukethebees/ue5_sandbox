#include "ioj/sim/fighter_navigation_scratch.h"

namespace ioj::sim::fighters {
NavigationScratch::NavigationScratch(std::pmr::memory_resource* const resource)
    : ready_fighter_indices{resource}
    , line_of_sight_starts{resource}
    , line_of_sight_ends{resource}
    , line_of_sight_results{resource}
    , trace_hits{resource}
    , blocked_fighter_indices{resource}
    , trace_fighter_indices{resource}
    , trace_choice_indices{resource}
    , observed_risk_tiers{resource} {}
}
