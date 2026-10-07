#pragma once

#include <ioj/levels/level_definition.h>
#include <SpaceGame/levels/LevelDefinition.h>

namespace ml::level_authoring {
[[nodiscard]] SPACEGAME_API auto to_native(FLevelDefinition const& definition)
    -> ::ioj::levels::LevelDefinition;
[[nodiscard]] SPACEGAME_API auto to_unreal(::ioj::levels::LevelDefinition definition)
    -> FLevelDefinition;
} // namespace ml::level_authoring
