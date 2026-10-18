#pragma once

#include <ioj/levels/level_definition.h>
#include <SpaceGame/levels/LevelDefinition.h>

namespace ioj::levels::authoring {
[[nodiscard]] SPACEGAME_API auto to_unreal(LevelId const& id) -> FLevelId;
// Decode and verify Unreal components before constructing a trusted native definition.
[[nodiscard]] SPACEGAME_API auto to_native(FLevelDraft const& definition)
    -> std::expected<LevelDefinition, Diagnostics>;
[[nodiscard]] SPACEGAME_API auto to_unreal(LevelDefinition const& definition) -> FLevelDraft;
} // namespace ioj::levels::authoring
