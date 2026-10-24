#include <ioj/sim/missions/objectives.h>

#include <limits>

namespace ioj::sim {
void Objectives::resize(std::size_t const count) {
    names_.resize(count);
    titles_.resize(count);
    teams_.resize(count);
    roots_.resize(count);
    leaves_.resize(count);
    composites_.resize(count);
    prerequisites_.resize(count);
    dependents_.resize(count);
    observers_.resize(count);
    team_results_.resize(count);
    outcomes_.resize(count);
    activation_ticks_.resize(count, std::numeric_limits<SimTick>::max());
    membership_positions_.resize(count);
}
}
