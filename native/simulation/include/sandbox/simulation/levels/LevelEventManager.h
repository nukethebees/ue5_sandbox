#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <sandbox/simulation/levels/CompiledLevelEvents.h>
#include <sandbox/simulation/levels/LevelSpawnManager.h>
#include <sandbox/simulation/sim_tick.h>

struct FTestMissionManager;

namespace ml {
class FLevelEventManager {
  public:
    FLevelEventManager(test_capital_ships::Simulation& capital_ships,
                       test_static_turrets::Simulation& turrets,
                       FTestMissionManager& mission_manager) noexcept;
    FLevelEventManager(FLevelEventManager const&) = delete;
    FLevelEventManager(FLevelEventManager&&) = delete;
    auto operator=(FLevelEventManager const&) -> FLevelEventManager& = delete;
    auto operator=(FLevelEventManager&&) -> FLevelEventManager& = delete;

    void initialise(FCompiledLevelEvents data, FRegistryEntityHandle player_handle = {});
    auto dispatch_tick(simulation::SimTick tick) -> bool;
    void configure_mission();
    auto get_entity_handle(std::int32_t entity_index) const -> FRegistryEntityHandle;
    auto has_future_spawns() const noexcept -> bool;
  private:
    FLevelInitialisationData initialisation_{};
    FLevelEventSchedule schedule_{};
    FLevelSpawnManager spawn_manager_;
    FTestMissionManager& mission_manager_;
    std::int32_t next_event_index_{};
    std::int32_t spawn_group_offset_{};
    std::int32_t mission_group_offset_{};
};
}
