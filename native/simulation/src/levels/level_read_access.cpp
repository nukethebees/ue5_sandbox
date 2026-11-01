#include <ioj/sim/levels/level_read_access.h>

#include <ioj/sim/levels/level_sim.h>
#include <ioj/sim/sim_clock.h>

#include <algorithm>

namespace ioj::sim {
auto LevelReadAccess::capitals() const -> capital_ships::Sim const& {
    return level_.capital_ships();
}
auto LevelReadAccess::fighters() const -> fighters::Sim const& {
    return level_.fighters();
}
auto LevelReadAccess::turrets() const -> turrets::Sim const& {
    return level_.turrets();
}
auto LevelReadAccess::spinners() const -> spinners::Sim const& {
    return level_.spinners();
}
auto LevelReadAccess::lasers() const -> lasers::Sim const& {
    return level_.lasers();
}
auto LevelReadAccess::player() const -> player::Sim const* {
    return level_.player_ship_simulation();
}
auto LevelReadAccess::clock() const -> SimClock const& {
    return level_.clock();
}
auto LevelReadAccess::entity_ledger() const -> EntityLedger const& {
    return level_.entity_ledger();
}
auto LevelReadAccess::interpolation_alpha() const -> double {
    auto const& clock{this->clock()};
    return std::clamp(clock.tick_loop.accumulator / clock.tick_period(), 0.0, 1.0);
}
} // namespace ioj::sim
