#pragma once

namespace ioj::sim {
class EntityLedger;
struct LevelSim;
struct SimClock;

namespace capital_ships {
struct Sim;
}
namespace fighters {
struct Sim;
}
namespace lasers {
struct Sim;
}
namespace player {
struct Sim;
}
namespace spinners {
struct Sim;
}
namespace turrets {
struct Sim;
}

// Borrow read-only simulations on demand. The level must outlive this facade.
// Storage views obtained from those simulations expire on advance or state mutation.
class LevelReadAccess {
  public:
    explicit LevelReadAccess(LevelSim const& level)
        : level_{level} {}

    auto capitals() const -> capital_ships::Sim const&;
    auto fighters() const -> fighters::Sim const&;
    auto turrets() const -> turrets::Sim const&;
    auto spinners() const -> spinners::Sim const&;
    auto lasers() const -> lasers::Sim const&;
    auto player() const -> player::Sim const*;
    auto clock() const -> SimClock const&;
    auto entity_ledger() const -> EntityLedger const&;
    auto interpolation_alpha() const -> double;
  private:
    LevelSim const& level_;
};
} // namespace ioj::sim
