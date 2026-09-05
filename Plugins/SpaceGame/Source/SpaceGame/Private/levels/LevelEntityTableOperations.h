#pragma once

#include <SpaceGame/levels/LevelDefinition.h>

namespace ml::level_entity_table_detail {
inline void append(FLevelEntityTable& entities, FEntitySpawnDefinition const& entity) {
    entities.ids.Add(entity.id);
    entities.archetypes.Add(entity.archetype);
    entities.teams.Add(entity.team);
    entities.positions.add(entity.position);
    entities.rotations.add(entity.rotation.Pitch, entity.rotation.Yaw, entity.rotation.Roll);
    entities.spawn_times_seconds.Add(entity.spawn_time_seconds);
}

inline auto get(FLevelEntityTableConstView const entities, int32 const index)
    -> FEntitySpawnDefinition {
    return {
        .id = entities.ids[index],
        .archetype = entities.archetypes[index],
        .team = entities.teams[index],
        .position = entities.positions[index],
        .rotation = FRotator{entities.rotations.pitches[index],
                             entities.rotations.yaws[index],
                             entities.rotations.rolls[index]},
        .spawn_time_seconds = entities.spawn_times_seconds[index],
    };
}

inline auto find_index(FLevelEntityTableConstView const entities, FLevelEntityId const id)
    -> int32 {
    auto const entity_count{entities.num()};
    for (int32 i{0}; i < entity_count; ++i) {
        if (entities.ids[i] == id) {
            return i;
        }
    }
    return INDEX_NONE;
}
}
