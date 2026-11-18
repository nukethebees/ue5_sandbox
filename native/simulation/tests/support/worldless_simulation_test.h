#pragma once
#include "simulation_fixture.h"

#include <functional>

namespace ioj::sim::tests {
class WorldlessSimulationTest {
  public:
    using time_type = LevelSim::time_type;
    explicit WorldlessSimulationTest(LevelSimInitData data);
    auto simulation() -> LevelSim& { return simulation_; }
    auto simulation() const -> LevelSim const& { return simulation_; }
    auto ledger() const -> EntityLedger const& { return simulation_.entity_ledger(); }
    auto time() const -> time_type { return simulation_.clock().simulation_time(); }
    void queue_damage(std::span<EntityId const> targets,
                      std::int32_t damage,
                      EntityId instigator = {});
    void queue_kills(std::span<EntityId const> targets, EntityId instigator = {});
    void advance(time_type dt);
    std::function<void(LevelSim&)> on_end_tick;
  private:
    LevelSim simulation_;
};
}
