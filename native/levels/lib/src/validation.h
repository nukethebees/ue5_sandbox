#pragma once

#include <ioj/levels/level_definition.h>

#include <unordered_set>

namespace ioj::levels::detail {
using IdSet = std::unordered_set<EntityId>;

auto validate_entities(LevelDefinitionComponents const& definition, Diagnostics& result) -> IdSet;
void validate_camera(LevelCameraDefinition const& camera,
                     IdSet const& entities,
                     Diagnostics& result);
void validate_collision_grid(sim::OverlapResponseConfig const& grid, Diagnostics& result);
}
