#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <ioj/sim/levels/compiled_level_events.h>
#include <ioj/sim/levels/level_spawn_manager.h>
#include <ioj/sim/sim_tick.h>

namespace ioj::sim {
struct MissionManager;

class LevelEventManager {
  public:
    LevelEventManager(capital_ships::Sim& capital_ships,
                      turrets::Sim& turrets,
                      spinners::Sim& spinners,
                      MissionManager& mission_manager) noexcept;
    LevelEventManager(LevelEventManager const&) = delete;
    LevelEventManager(LevelEventManager&&) = delete;
    auto operator=(LevelEventManager const&) -> LevelEventManager& = delete;
    auto operator=(LevelEventManager&&) -> LevelEventManager& = delete;

    void initialise(CompiledLevelEvents data, EntityUniqueId player_id = {});
    void execute_tick(SimTick tick);
    auto get_spawned_ids() const -> std::span<EntityUniqueId const>;
    void configure_mission();
    auto get_entity_id(std::int32_t entity_index) const -> EntityUniqueId;
    auto has_future_spawns() const noexcept -> bool;
  private:
    LevelInitialisationData initialisation_{};
    LevelEventSchedule schedule_{};
    LevelSpawnManager spawn_manager_;
    MissionManager& mission_manager_;
    std::int32_t next_event_index_{};
    std::int32_t spawn_group_offset_{};
    std::int32_t mission_group_offset_{};
};
}
