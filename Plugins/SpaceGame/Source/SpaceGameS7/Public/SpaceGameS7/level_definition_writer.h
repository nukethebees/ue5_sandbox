#pragma once

#include <SpaceGame/levels/LevelDefinition.h>

#include <expected>

namespace ioj::levels::authoring {
SPACEGAMES7_API auto emit_editor_level_source(ml::FLevelDefinition const& definition)
    -> std::expected<FString, FString>;
}
