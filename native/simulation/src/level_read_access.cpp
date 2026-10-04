#include <ioj/sim/level_read_access.h>

#include <ioj/sim/level_sim.h>

#include <algorithm>

namespace ioj::sim {
auto LevelReadAccess::get_capitals() const -> capital_ships::Sim const& {
    return level_.get_capital_ships();
}
auto LevelReadAccess::get_fighters() const -> fighters::Sim const& {
    return level_.get_fighters();
}
auto LevelReadAccess::get_turrets() const -> turrets::Sim const& {
    return level_.get_turrets();
}
auto LevelReadAccess::get_spinners() const -> spinners::Sim const& {
    return level_.get_spinners();
}
auto LevelReadAccess::get_lasers() const -> lasers::Sim const& {
    return level_.get_lasers();
}
auto LevelReadAccess::get_player() const -> player::Sim const* {
    return level_.get_player_ship_simulation();
}
auto LevelReadAccess::get_clock() const -> SimClock const& {
    return level_.get_clock();
}
auto LevelReadAccess::frame_sequence() const -> std::uint64_t {
    return level_.frame_sequence_;
}
auto LevelReadAccess::interpolation_alpha() const -> double {
    auto const& clock{get_clock()};
    return std::clamp(clock.tick_loop.accumulator / clock.get_tick_period(), 0.0, 1.0);
}
} // namespace ioj::sim
