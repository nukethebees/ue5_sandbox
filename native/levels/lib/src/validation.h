#pragma once

#include "entity_validation_state.h"
#include <ioj/levels/level_definition.h>

#include <unordered_set>

namespace ioj::levels::detail {

struct MissionReferences {
    IdSet survivors{};
    IdSet required{};
};

auto validate_entities(LevelDefinition const& definition, Diagnostics& result)
    -> EntityValidationState;
// Return only references to declared entities, even when other validation fails.
auto validate_mission(LevelMissionDefinition const& mission,
                      EntityValidationState const& entities,
                      Diagnostics& result) -> MissionReferences;
// Consume the initial role sets produced by validate_mission, or empty sets without a mission.
void validate_mission_events(LevelDefinition const& definition,
                             EntityValidationState const& entities,
                             MissionReferences references,
                             Diagnostics& result);
void validate_camera(LevelCameraDefinition const& camera,
                     EntityValidationState const& entities,
                     Diagnostics& result);
void validate_collision_grid(LevelCollisionGridDefinition const& grid, Diagnostics& result);
}
