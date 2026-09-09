#pragma once

#include <SpaceGame/levels/CompiledLevelEvents.h>
#include <SpaceGame/levels/LevelSpawnManager.h>

struct FTestMissionManager;

namespace ml {
class SPACEGAME_API FLevelEventManager {
  public:
    FLevelEventManager(test_capital_ships::Simulation& capital_ships,
                       test_static_turrets::Simulation& turrets,
                       FTestMissionManager& mission_manager) noexcept;
    FLevelEventManager(FLevelEventManager const&) = delete;
    FLevelEventManager(FLevelEventManager&&) = delete;
    auto operator=(FLevelEventManager const&) -> FLevelEventManager& = delete;
    auto operator=(FLevelEventManager&&) -> FLevelEventManager& = delete;

    void initialise(FCompiledLevelEvents data, FRegistryEntityHandle player_handle = {});
    auto dispatch_tick(uint64 tick) -> bool;
    void configure_mission();
    auto get_entity_handle(int32 entity_index) const -> FRegistryEntityHandle;
    auto has_future_spawns() const noexcept -> bool;
  private:
    FLevelInitialisationData initialisation_{};
    FLevelEventSchedule schedule_{};
    FLevelSpawnManager spawn_manager_;
    FTestMissionManager& mission_manager_;
    int32 next_event_index_{};
    int32 spawn_group_offset_{};
    int32 mission_group_offset_{};
};
}
