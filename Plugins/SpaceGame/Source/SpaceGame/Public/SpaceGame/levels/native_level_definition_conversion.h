#pragma once

#include <ioj/levels/identifiers.h>
#include <SpaceGameSimulation/levels/LevelTypes.h>

namespace ioj::levels::authoring {
[[nodiscard]] SPACEGAME_API auto to_unreal(LevelId const& id) -> FLevelId;
} // namespace ioj::levels::authoring
