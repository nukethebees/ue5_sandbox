#pragma once

#include <ioj/levels/level_definition.h>
#include <SpaceGame/levels/LevelDefinition.h>

namespace ioj::levels::authoring {
[[nodiscard]] SPACEGAME_API auto to_native(ml::FLevelDefinition const& definition)
    -> ::ioj::levels::LevelDefinition;
[[nodiscard]] SPACEGAME_API auto to_unreal(::ioj::levels::LevelDefinition definition)
    -> ml::FLevelDefinition;
} // namespace ioj::levels::authoring
