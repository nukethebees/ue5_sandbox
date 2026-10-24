#include <ioj/sim/missions/objectives.h>

#include <limits>

namespace ioj::sim {
Objectives::Objectives(std::size_t const count)
    : names_(count)
    , titles_(count)
    , teams_(count)
    , roots_(count)
    , leaves_(count)
    , composites_(count)
    , prerequisites_(count)
    , dependents_(count)
    , observers_(count)
    , team_results_(count)
    , outcomes_(count)
    , activation_ticks_(count, std::numeric_limits<SimTick>::max())
    , membership_positions_(count) {}
}
