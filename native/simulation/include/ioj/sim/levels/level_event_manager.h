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
class ExprManager;

class LevelEventManager {
  public:
    LevelEventManager(capital_ships::Sim& capital_ships,
                      fighters::Sim& fighters,
                      turrets::Sim& turrets,
                      spinners::Sim& spinners,
                      std::span<Transform3d const> fighter_spawn_transforms,
                      CompiledLevelEvents data);
    LevelEventManager(LevelEventManager const&) = delete;
    LevelEventManager(LevelEventManager&&) = delete;
    auto operator=(LevelEventManager const&) -> LevelEventManager& = delete;
    auto operator=(LevelEventManager&&) -> LevelEventManager& = delete;

    void begin(SpawnContext context, EntityId player_id = {});
    void activate(ExprManager& expressions);
    void execute_tick(ExprManager& expressions, SpawnContext context);
    void queue_fighter_spawns(float dt,
                              FrameMemoryResource& scratch_resource,
                              std::span<Team const> participating_teams) {
        spawn_manager_.queue_fighter_spawns(dt, scratch_resource, participating_teams);
    }
    auto spawned_ids() const -> std::span<EntityId const>;
    auto entity_ids() const -> std::span<EntityId const> { return spawn_manager_.entity_ids(); }
    auto entity_id(LevelEntityIndex entity_index) const -> EntityId;
    auto participating_teams(std::optional<Team> player_team) const -> TeamList {
        return schedule_.get_participating_teams(player_team);
    }
  private:
    LevelEventSchedule schedule_{};
    LevelSpawnManager spawn_manager_;
    LevelEntityIndex player_entity_index_{invalid_level_entity_index};
};
}
