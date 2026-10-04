#include <ioj/sim/level_read_access.h>

#include <ioj/sim/level_sim.h>

#include <algorithm>

namespace ioj::sim {
auto LevelReadAccess::get_capitals() const -> CapitalReadView {
    return level_.get_capital_ships().get_read_view();
}
auto LevelReadAccess::get_fighters() const -> FighterReadView {
    return level_.get_fighters().get_read_view();
}
auto LevelReadAccess::get_turrets() const -> TurretReadView {
    return level_.get_turrets().get_read_view();
}
auto LevelReadAccess::get_spinners() const -> SpinnerReadView {
    return level_.get_spinners().get_read_view();
}
auto LevelReadAccess::get_lasers() const -> LaserReadView {
    return level_.get_lasers().get_read_view();
}
auto LevelReadAccess::get_player() const -> std::optional<PlayerReadView> {
    auto const* player{level_.get_player_ship_simulation()};
    return player ? std::optional{player->get_read_view()} : std::nullopt;
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
