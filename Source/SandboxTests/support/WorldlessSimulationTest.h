#pragma once

#include <SandboxCore/test_timeline.h>
#include <SpaceGame/simulation/LevelSimulation.h>

#include <Misc/Optional.h>

class USpaceGameLevelConfig;

namespace ml::test_space_ship {
struct FPlayerSpawnData;
}

namespace ml {
auto make_worldless_simulation_test_data(USpaceGameLevelConfig const& config)
    -> FLevelSimulationInitData;
auto make_worldless_player_spawn(USpaceGameLevelConfig const& config,
                                 FTransform const& transform = FTransform::Identity)
    -> ml::test_space_ship::FPlayerSpawnData;
auto add_worldless_capital_spawn(FLevelSimulationInitData& data,
                                 FVector3f location,
                                 ETestTeam team,
                                 int32 target_spawn_index = INDEX_NONE,
                                 float initial_spawn_delay = 0.f,
                                 float spawn_cooldown = 60.f,
                                 int32 health = INDEX_NONE) -> int32;

class FWorldlessSimulationTest {
  public:
    using time_type = FLevelSimulation::time_type;

    explicit FWorldlessSimulationTest(FLevelSimulationInitData data);
    FWorldlessSimulationTest(FWorldlessSimulationTest const&) = delete;
    FWorldlessSimulationTest(FWorldlessSimulationTest&&) = delete;
    auto operator=(FWorldlessSimulationTest const&) -> FWorldlessSimulationTest& = delete;
    auto operator=(FWorldlessSimulationTest&&) -> FWorldlessSimulationTest& = delete;

    auto get_simulation() -> FLevelSimulation& { return simulation_; }
    auto get_simulation() const -> FLevelSimulation const& { return simulation_; }
    auto get_registry() -> FTestEntityRegistry& { return simulation_.get_entity_registry(); }
    auto get_registry() const -> FTestEntityRegistry const& {
        return simulation_.get_entity_registry();
    }
    auto get_time() const -> time_type { return simulation_.get_clock().get_simulation_time(); }

    void finish_initialisation();
    void queue_damage(TConstArrayView<FRegistryEntityHandle> targets,
                      int32 damage,
                      FRegistryEntityHandle instigator = {});
    void queue_kills(TConstArrayView<FRegistryEntityHandle> targets,
                     FRegistryEntityHandle instigator = {});
    auto run_until_timeline_finished(time_type maximum_time) -> bool;

    TFunction<void(FLevelSimulation&)> on_end_tick;
    FTestTimeline timeline;
  private:
    FLevelSimulation simulation_;
};
}
