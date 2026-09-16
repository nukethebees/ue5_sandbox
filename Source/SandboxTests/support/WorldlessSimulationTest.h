#pragma once
#include <CoreMinimal.h>
#include <SpaceGameSimulation/entities/TestTeam.h>
#include <span>

#include <ioj/sim/level_sim.h>
#include <sandbox/core/test_timeline.h>

#include <Misc/Optional.h>

class USpaceGameLevelConfig;

namespace ioj::sim::player {
struct PlayerSpawnData;
}

namespace ml {
auto make_worldless_simulation_test_data(USpaceGameLevelConfig const& config)
    -> ::ioj::sim::LevelSimInitData;
auto make_worldless_player_spawn(USpaceGameLevelConfig const& config,
                                 FTransform const& transform = FTransform::Identity)
    -> ::ioj::sim::player::PlayerSpawnData;
auto add_worldless_player_spawn(::ioj::sim::LevelSimInitData& data,
                                ::ioj::sim::player::PlayerSpawnData spawn) -> int32;
auto add_worldless_capital_spawn(::ioj::sim::LevelSimInitData& data,
                                 FVector3f location,
                                 ETestTeam team,
                                 int32 target_entity_index = INDEX_NONE,
                                 float initial_spawn_delay = 0.f,
                                 float spawn_cooldown = 60.f,
                                 int32 health = INDEX_NONE) -> int32;

class FWorldlessSimulationTest {
  public:
    using time_type = ::ioj::sim::LevelSim::time_type;

    explicit FWorldlessSimulationTest(::ioj::sim::LevelSimInitData data);
    FWorldlessSimulationTest(FWorldlessSimulationTest const&) = delete;
    FWorldlessSimulationTest(FWorldlessSimulationTest&&) = delete;
    auto operator=(FWorldlessSimulationTest const&) -> FWorldlessSimulationTest& = delete;
    auto operator=(FWorldlessSimulationTest&&) -> FWorldlessSimulationTest& = delete;

    auto get_simulation() -> ::ioj::sim::LevelSim& { return simulation_; }
    auto get_simulation() const -> ::ioj::sim::LevelSim const& { return simulation_; }
    auto get_registry() const -> ::ioj::sim::EntityRegistry const& {
        return simulation_.get_entity_registry();
    }
    auto get_time() const -> time_type { return simulation_.get_clock().get_simulation_time(); }

    void finish_initialisation();
    void queue_damage(std::span<::ioj::sim::RegistryEntityHandle const> targets,
                      int32 damage,
                      ::ioj::sim::RegistryEntityHandle instigator = {});
    void queue_kills(std::span<::ioj::sim::RegistryEntityHandle const> targets,
                     ::ioj::sim::RegistryEntityHandle instigator = {});
    void queue_damage(std::span<::ioj::sim::EntityUniqueId const> targets,
                      int32 damage,
                      ::ioj::sim::EntityUniqueId instigator = {});
    void queue_kills(std::span<::ioj::sim::EntityUniqueId const> targets,
                     ::ioj::sim::EntityUniqueId instigator = {});
    void advance(time_type dt);
    auto run_until_timeline_finished(time_type maximum_time) -> bool;

    TFunction<void(::ioj::sim::LevelSim&)> on_end_tick;
    FTestTimeline timeline;
  private:
    ::ioj::sim::LevelSim simulation_;
};
}
