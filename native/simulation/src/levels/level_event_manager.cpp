#include "ioj/sim/levels/level_event_manager.h"

#include <ioj/sim/expressions/expr_manager.h>
#include <ioj/sim/levels/spawn_context.h>

#include <algorithm>
#include <span>
#include <utility>

namespace ioj::sim {
LevelEventManager::LevelEventManager(capital_ships::Sim& capital_ships,
                                     fighters::Sim& fighters,
                                     turrets::Sim& turrets,
                                     spinners::Sim& spinners,
                                     std::span<Transform3d const> const fighter_spawn_transforms,
                                     CompiledLevelEvents data)
    : schedule_{std::move(data.schedule)}
    , spawn_manager_{capital_ships, fighters, turrets, spinners, fighter_spawn_transforms}
    , player_entity_index_{data.initialisation.player_entity_index} {
    spawn_manager_.initialise(data.initialisation.entity_count,
                              schedule_.capital_spawns.const_view(),
                              schedule_.turret_spawns.const_view(),
                              schedule_.spinner_spawns.const_view());
}

void LevelEventManager::begin(SpawnContext const context, EntityId const player_id) {
    if (player_entity_index_ != invalid_level_entity_index) {
        spawn_manager_.set_entity_id(player_entity_index_, player_id);
    }
    spawn_manager_.spawn(schedule_.initial_spawn_groups.const_view(), context);
}

void LevelEventManager::activate(ExprManager& expressions) {
    for (auto const& event : schedule_.pending_spawns) {
        expressions.activate(event.condition, SimTick{});
    }
}

void LevelEventManager::execute_tick(ExprManager& expressions, SpawnContext const context) {
    spawn_manager_.reset_tick_output();
    std::erase_if(schedule_.pending_spawns, [&](LevelSpawnEvent const& event) {
        auto const result{expressions.result(event.condition)};
        if (result == ExprResult::Pending) {
            return false;
        }

        if (result == ExprResult::True) {
            spawn_manager_.spawn(schedule_.spawn_groups.const_view(event.spawn_groups.offset,
                                                                   event.spawn_groups.count),
                                 context);
        }
        expressions.deactivate(event.condition);
        return true;
    });
}

auto LevelEventManager::spawned_ids() const -> std::span<EntityId const> {
    return spawn_manager_.spawned_ids();
}

auto LevelEventManager::entity_id(LevelEntityIndex const entity_index) const -> EntityId {
    return spawn_manager_.id(entity_index);
}

}
