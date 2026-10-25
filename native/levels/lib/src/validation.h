#pragma once

#include "entity_validation_state.h"
#include <ioj/levels/level_definition.h>

#include <unordered_set>

namespace ioj::levels::detail {

auto validate_entities(LevelDefinitionComponents const& definition, Diagnostics& result)
    -> EntityValidationState;
void validate_camera(LevelCameraDefinition const& camera,
                     EntityValidationState const& entities,
                     Diagnostics& result);
void validate_collision_grid(sim::OverlapResponseConfig const& grid, Diagnostics& result);
}
