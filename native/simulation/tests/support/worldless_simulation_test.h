#pragma once
#include "simulation_fixture.h"
#include <ioj/test_timeline.h>

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
    auto run_until_timeline_finished(time_type maximum_time) -> bool;
    std::function<void(LevelSim&)> on_end_tick;
    FTestTimeline timeline;
    fighters::NavigationCounters navigation_counters;
  private:
    LevelSim simulation_;
};
}
