#pragma once

#include <sandbox/simulation/levels/LevelDefinition.h>
#include <SpaceGame/levels/LevelDefinition.h>

namespace ml::level_authoring {
[[nodiscard]] auto to_native(FLevelDefinition const& definition) -> LevelDefinition;
}
