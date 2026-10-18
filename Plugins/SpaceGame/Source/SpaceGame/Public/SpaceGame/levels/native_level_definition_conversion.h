#pragma once

#include <ioj/levels/level_definition.h>
#include <SpaceGame/levels/LevelDefinition.h>

namespace ioj::levels::authoring {
// Decode and verify Unreal components before constructing a trusted native definition.
[[nodiscard]] SPACEGAME_API auto to_native(ml::FLevelDefinition const& definition)
    -> std::expected<LevelDefinition, Diagnostics>;
[[nodiscard]] SPACEGAME_API auto to_unreal(LevelDefinition const& definition)
    -> ml::FLevelDefinition;
} // namespace ioj::levels::authoring
