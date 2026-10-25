#pragma once

#include <ioj/levels/entity_spawn_definition.h>
#include <ioj/levels/level_camera_definition.h>
#include <ioj/levels/level_metadata.h>
#include <ioj/levels/mission_definition.h>
#include <ioj/settings/settings.h>

#include <optional>
#include <vector>

namespace ioj::levels {
// Assemble mutable authoring inputs before LevelDefinition::make validates them.
struct LevelDefinitionComponents {
    LevelMetadata metadata{};
    std::vector<LevelId> unlock_level_ids{};
    EntityId player_entity_id{};
    std::optional<LevelCameraDefinition> camera{};
    std::optional<MissionDefinition> mission{};
    std::vector<EntitySpawnDefinition> entities{};
    settings::Settings settings{};
};
}
