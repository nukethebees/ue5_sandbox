#include "ioj/sim/fighter_firing_scratch.h"

namespace ioj::sim::fighters {
FiringScratch::FiringScratch(std::pmr::memory_resource* const resource)
    : new_lasers{resource}
    , aiming_dot_products{resource}
    , can_fire{resource}
    , line_of_sight_starts{resource}
    , line_of_sight_ends{resource}
    , line_of_sight_results{resource}
    , ignored_entities{resource}
    , position_fighter_indices{resource}
    , position_candidates{resource} {}
}
