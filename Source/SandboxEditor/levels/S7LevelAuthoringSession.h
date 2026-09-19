#pragma once

#include <SpaceGame/levels/LevelDefinition.h>

#include <CoreMinimal.h>

#include <expected>

class AS7LevelAuthoringDocument;
class ULevel;

namespace ml::editor {
SANDBOXEDITOR_API auto find_level_authoring_document(ULevel const& level)
    -> std::expected<AS7LevelAuthoringDocument*, FString>;
SANDBOXEDITOR_API auto create_level_authoring_document(ULevel& level)
    -> std::expected<AS7LevelAuthoringDocument*, FString>;
SANDBOXEDITOR_API auto adopt_unbound_level_entities(ULevel& level,
                                                    AS7LevelAuthoringDocument& document)
    -> std::expected<int32, FString>;
SANDBOXEDITOR_API auto collect_s7_editor_level(ULevel const& level,
                                               AS7LevelAuthoringDocument const& document)
    -> std::expected<FLevelDefinition, FString>;
}
