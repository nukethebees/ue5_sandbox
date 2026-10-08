#pragma once

#include "entity_validation_state.h"
#include <ioj/levels/level_definition.h>

#include <unordered_set>

namespace ioj::levels::detail {

auto validate_entities(LevelDefinition const& definition,
                       std::unordered_set<TeamId> const& declared_teams,
                       Diagnostics& result) -> EntityValidationState;
void validate_mission(LevelMissionDefinition const& mission,
                      EntityValidationState const& entities,
                      Diagnostics& result);
void validate_mission_events(LevelDefinition const& definition,
                             EntityValidationState const& entities,
                             Diagnostics& result);
void validate_camera(LevelCameraDefinition const& camera,
                     EntityValidationState const& entities,
                     Diagnostics& result);
void validate_collision_grid(LevelCollisionGridDefinition const& grid, Diagnostics& result);
}
