#pragma once
#include <sandbox/core/test_timeline.h>
#include "simulation_fixture.h"

namespace ml::simulation_tests {
class WorldlessSimulationTest {
  public:
    using time_type = FLevelSimulation::time_type;
    explicit WorldlessSimulationTest(FLevelSimulationInitData data);
    auto get_simulation() -> FLevelSimulation& { return simulation_; }
    auto get_simulation() const -> FLevelSimulation const& { return simulation_; }
    auto get_registry() -> FTestEntityRegistry& { return simulation_.get_entity_registry(); }
    auto get_registry() const -> FTestEntityRegistry const& {
        return simulation_.get_entity_registry();
    }
    auto get_time() const -> time_type { return simulation_.get_clock().get_simulation_time(); }
    void finish_initialisation() { simulation_.finish_initialisation(); }
    void queue_damage(std::span<FRegistryEntityHandle const> targets,
                      std::int32_t damage,
                      FRegistryEntityHandle instigator = {});
    void queue_kills(std::span<FRegistryEntityHandle const> targets,
                     FRegistryEntityHandle instigator = {});
    auto run_until_timeline_finished(time_type maximum_time) -> bool;
    std::function<void(FLevelSimulation&)> on_end_tick;
    FTestTimeline timeline;
  private:
    FLevelSimulation simulation_;
};
}
