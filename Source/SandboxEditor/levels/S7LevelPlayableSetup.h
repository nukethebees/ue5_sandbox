#pragma once

#include <CoreMinimal.h>

#include <expected>

class AS7LevelAuthoringDocument;
class ULevel;
class UWorld;

namespace ml::editor {
// Seed the existing authored runtime before PIE actors begin play. Other worlds/maps are ignored.
SANDBOXEDITOR_API auto prepare_s7_level_for_pie(UWorld& world) -> std::expected<void, FString>;
SANDBOXEDITOR_API auto set_up_playable_s7_level(ULevel& level, AS7LevelAuthoringDocument& document)
    -> std::expected<FString, FString>;
SANDBOXEDITOR_API auto validate_playable_s7_level(ULevel& level,
                                                  AS7LevelAuthoringDocument& document)
    -> std::expected<FString, FString>;
}
