#pragma once
#include <ioj/sim/levels/level_sim.h>
#include <ioj/test_timeline.h>
#include <SpaceGamePresentation/entities/TestTeam.h>

#include <CoreMinimal.h>
#include <Misc/Optional.h>

#include <span>

class USpaceGameLevelConfig;

namespace ioj::sim::player {
struct PlayerSpawnData;
}

namespace ioj {
auto make_worldless_simulation_test_data(USpaceGameLevelConfig const& config)
    -> ::ioj::sim::LevelSimInitData;
auto make_worldless_player_spawn(USpaceGameLevelConfig const& config,
                                 FTransform const& transform = FTransform::Identity)
    -> ::ioj::sim::player::PlayerSpawnData;
auto add_worldless_player_spawn(sim::LevelSimInitData& data, sim::player::PlayerSpawnData spawn)
    -> uint32;
auto add_worldless_capital_spawn(sim::LevelSimInitData& data,
                                 FVector3f location,
                                 ETestTeam team,
                                 uint32 target_entity_index = sim::invalid_level_entity_index,
                                 float initial_spawn_delay = 0.f,
                                 float spawn_cooldown = 60.f,
                                 int32 health = INDEX_NONE) -> uint32;

class FWorldlessSimulationTest {
  public:
    using time_type = sim::LevelSim::time_type;

    explicit FWorldlessSimulationTest(sim::LevelSimInitData data);
    FWorldlessSimulationTest(FWorldlessSimulationTest const&) = delete;
    FWorldlessSimulationTest(FWorldlessSimulationTest&&) = delete;
    auto operator=(FWorldlessSimulationTest const&) -> FWorldlessSimulationTest& = delete;
    auto operator=(FWorldlessSimulationTest&&) -> FWorldlessSimulationTest& = delete;

    auto get_simulation() -> ::ioj::sim::LevelSim& { return simulation_; }
    auto get_simulation() const -> ::ioj::sim::LevelSim const& { return simulation_; }
    auto get_ledger() const -> ::ioj::sim::EntityLedger const& {
        return simulation_.get_entity_ledger();
    }
    auto get_time() const -> time_type { return simulation_.get_clock().get_simulation_time(); }

    void finish_initialisation();
    void queue_damage(std::span<sim::EntityId const> targets,
                      int32 damage,
                      sim::EntityId instigator = {});
    void queue_kills(std::span<sim::EntityId const> targets, sim::EntityId instigator = {});
    void advance(time_type dt);
    auto run_until_timeline_finished(time_type maximum_time) -> bool;

    TFunction<void(sim::LevelSim&)> on_end_tick;
    FTestTimeline timeline;
  private:
    sim::GameMemory game_memory_;
    sim::LevelSim simulation_;
};
}
