#pragma once
#include <ioj/sim/capital_ships/sim.h>
#include <ioj/sim/fighters/sim.h>
#include <ioj/sim/lasers/sim.h>
#include <ioj/sim/player/sim.h>
#include <ioj/sim/spinners/sim.h>
#include <ioj/sim/turrets/sim.h>

#include <cstdint>

namespace ioj::sim {
struct LevelSim;
struct SimClock;

// Borrow read-only simulations on demand. The level must outlive this facade.
// Storage views obtained from those simulations expire on advance or state mutation.
class LevelReadAccess {
  public:
    explicit LevelReadAccess(LevelSim const& level)
        : level_{level} {}

    auto get_capitals() const -> capital_ships::Sim const&;
    auto get_fighters() const -> fighters::Sim const&;
    auto get_turrets() const -> turrets::Sim const&;
    auto get_spinners() const -> spinners::Sim const&;
    auto get_lasers() const -> lasers::Sim const&;
    auto get_player() const -> player::Sim const*;
    auto get_clock() const -> SimClock const&;
    auto frame_sequence() const -> std::uint64_t;
    auto interpolation_alpha() const -> double;
  private:
    LevelSim const& level_;
};
} // namespace ioj::sim
