#pragma once

#include <ioj/levels/level_definition.h>
#include <SpaceGame/levels/LevelDefinition.h>

namespace ioj::levels::authoring {
// Decode Unreal's column structure and symbolic identifiers. The result is editable
// data; validate it at the consumer boundary before treating it as a playable level.
[[nodiscard]] SPACEGAME_API auto to_native(ml::FLevelDefinition const& definition)
    -> std::expected<LevelDefinition, Diagnostics>;
[[nodiscard]] SPACEGAME_API auto to_unreal(LevelDefinition const& definition)
    -> ml::FLevelDefinition;
} // namespace ioj::levels::authoring
