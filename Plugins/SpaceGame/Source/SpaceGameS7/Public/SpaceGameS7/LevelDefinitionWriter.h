#pragma once

#include <SpaceGame/levels/LevelDefinition.h>

#include <expected>

namespace ml::s7 {
SPACEGAMES7_API auto emit_initial_level_source(FLevelDefinition const& definition)
    -> std::expected<FString, FString>;
}
