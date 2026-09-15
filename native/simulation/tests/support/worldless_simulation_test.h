#pragma once
#include <sandbox/core/test_timeline.h>
#include "simulation_fixture.h"

namespace ioj::sim::tests {
class WorldlessSimulationTest {
  public:
    using time_type = LevelSim::time_type;
    explicit WorldlessSimulationTest(LevelSimInitData data);
    auto get_simulation() -> LevelSim& { return simulation_; }
    auto get_simulation() const -> LevelSim const& { return simulation_; }
    auto get_registry() const -> EntityRegistry const& { return simulation_.get_entity_registry(); }
    auto get_time() const -> time_type { return simulation_.get_clock().get_simulation_time(); }
    void finish_initialisation() { simulation_.finish_initialisation(); }
    void queue_damage(std::span<RegistryEntityHandle const> targets,
                      std::int32_t damage,
                      RegistryEntityHandle instigator = {});
    void queue_kills(std::span<RegistryEntityHandle const> targets,
                     RegistryEntityHandle instigator = {});
    auto run_until_timeline_finished(time_type maximum_time) -> bool;
    std::function<void(LevelSim&)> on_end_tick;
    FTestTimeline timeline;
  private:
    LevelSim simulation_;
};
}
