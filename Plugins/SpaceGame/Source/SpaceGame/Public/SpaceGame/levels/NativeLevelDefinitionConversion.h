#pragma once

#include <SpaceGame/levels/LevelDefinition.h>

#include <sandbox/simulation/levels/LevelDefinition.h>

namespace ml::level_authoring {
[[nodiscard]] SPACEGAME_API auto to_native(FLevelDefinition const& definition) -> LevelDefinition;
[[nodiscard]] SPACEGAME_API auto to_unreal(LevelDefinition definition) -> FLevelDefinition;
} // namespace ml::level_authoring
