#pragma once

#include <ioj/levels/level_definition.h>
#include <SpaceGame/levels/LevelDefinition.h>

namespace ioj::levels::authoring {
[[nodiscard]] SPACEGAME_API auto to_unreal(LevelId const& id) -> ioj::FLevelId;
// Decode and verify Unreal components before constructing a trusted native definition.
[[nodiscard]] SPACEGAME_API auto to_native(ioj::FLevelDraft const& definition)
    -> std::expected<LevelDefinition, Diagnostics>;
[[nodiscard]] SPACEGAME_API auto to_unreal(LevelDefinition const& definition) -> ioj::FLevelDraft;
} // namespace ioj::levels::authoring
