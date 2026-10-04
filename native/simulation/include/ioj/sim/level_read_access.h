#pragma once
#include <ioj/sim/player/player_read_view.h>
#include <ioj/sim/system_read_views.h>

#include <cstdint>
#include <optional>

namespace ioj::sim {
struct LevelSim;
struct SimClock;

// Borrow the level and obtain current typed views on demand. This is not a frame snapshot.
// The level must outlive this facade; returned buffers expire on advance or state mutation.
class LevelReadAccess {
  public:
    explicit LevelReadAccess(LevelSim const& level)
        : level_{level} {}

    auto get_capitals() const -> CapitalReadView;
    auto get_fighters() const -> FighterReadView;
    auto get_turrets() const -> TurretReadView;
    auto get_spinners() const -> SpinnerReadView;
    auto get_lasers() const -> LaserReadView;
    auto get_player() const -> std::optional<PlayerReadView>;
    auto get_clock() const -> SimClock const&;
    auto frame_sequence() const -> std::uint64_t;
    auto interpolation_alpha() const -> double;
  private:
    LevelSim const& level_;
};
} // namespace ioj::sim
