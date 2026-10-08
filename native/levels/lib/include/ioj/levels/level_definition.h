#pragma once

#include <ioj/levels/diagnostics.h>
#include <ioj/levels/entity_spawn_definition.h>
#include <ioj/levels/level_camera_definition.h>
#include <ioj/levels/level_collision_grid_definition.h>
#include <ioj/levels/level_metadata.h>
#include <ioj/levels/level_mission_definition.h>
#include <ioj/levels/level_mission_objective_event.h>

#include <optional>
#include <vector>

namespace ioj::levels {
struct LevelDefinition {
    LevelMetadata metadata{};
    std::vector<LevelId> unlock_level_ids{};
    EntityId player_entity_id{};
    std::optional<LevelCameraDefinition> camera{};
    std::optional<LevelCollisionGridDefinition> collision_grid{};
    std::optional<LevelMissionDefinition> mission{};
    std::vector<LevelMissionObjectiveEvent> mission_events{};
    // Include declared participants even when they have no authored entities.
    std::vector<TeamId> teams{};
    std::vector<EntitySpawnDefinition> entities{};
};

[[nodiscard]] auto validate_level(LevelDefinition const& definition)
    -> std::expected<void, Diagnostics>;
} // namespace ioj::levels
