#pragma once

#include <SpaceGame/levels/LevelDefinition.h>

#include <ioj/sim/levels/level_definition.h>

namespace ml::level_authoring {
[[nodiscard]] SPACEGAME_API auto to_native(FLevelDefinition const& definition)
    -> ::ioj::sim::levels::LevelDefinition;
[[nodiscard]] SPACEGAME_API auto to_unreal(::ioj::sim::levels::LevelDefinition definition)
    -> FLevelDefinition;
} // namespace ml::level_authoring
