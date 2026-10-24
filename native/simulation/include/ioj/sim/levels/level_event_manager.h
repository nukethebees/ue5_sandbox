#pragma once
#include <ioj/sim/levels/compiled_level_events.h>
#include <ioj/sim/levels/level_entity_index.h>
#include <ioj/sim/levels/level_spawn_manager.h>
#include <ioj/sim/sim_tick.h>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ioj::sim {

class LevelEventManager {
  public:
    LevelEventManager(capital_ships::Sim& capital_ships,
                      turrets::Sim& turrets,
                      spinners::Sim& spinners) noexcept;
    LevelEventManager(LevelEventManager const&) = delete;
    LevelEventManager(LevelEventManager&&) = delete;
    auto operator=(LevelEventManager const&) -> LevelEventManager& = delete;
    auto operator=(LevelEventManager&&) -> LevelEventManager& = delete;

    void initialise(CompiledLevelEvents data, EntityId player_id = {});
    void execute_tick(SimTick tick);
    auto get_spawned_ids() const -> std::span<EntityId const>;
    auto take_mission() -> std::optional<LevelMissionInitialisationData> {
        return std::exchange(initialisation_.mission, {});
    }
    auto get_entity_ids() const -> std::span<EntityId const> {
        return spawn_manager_.get_entity_ids();
    }
    auto get_entity_id(LevelEntityIndex entity_index) const -> EntityId;
    auto has_future_spawns() const noexcept -> bool;
  private:
    LevelInitialisationData initialisation_{};
    LevelEventSchedule schedule_{};
    LevelSpawnManager spawn_manager_;
    std::uint32_t next_event_index_{};
    std::uint32_t spawn_group_offset_{};
};
}
