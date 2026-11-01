#include <ioj/sim/missions/objectives.h>

#include <limits>

namespace ioj::sim {
Objectives::Objectives(std::size_t const count)
    : identifiers_(count)
    , display_names_(count)
    , assigned_teams_(count)
    , roots_(count)
    , leaf_ranges_(count)
    , composite_ranges_(count)
    , dependent_ranges_(count)
    , observer_ranges_(count)
    , team_results_(count)
    , prerequisites_(count)
    , outcomes_(count)
    , activation_ticks_(count, SimTick{std::numeric_limits<SimTick::value_type>::max()})
    , membership_positions_(count) {}
}
