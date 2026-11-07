#pragma once

#include <ioj/levels/authoring/level_inputs.h>
#include <ioj/levels/level_definition.h>

#include <CoreMinimal.h>

#include <expected>

class AS7LevelAuthoringDocument;
class ULevel;

namespace ioj::editor {
SANDBOXEDITOR_API auto find_level_authoring_document(ULevel const& level)
    -> std::expected<AS7LevelAuthoringDocument*, FString>;
SANDBOXEDITOR_API auto create_level_authoring_document(ULevel& level, FString source_path = {})
    -> std::expected<AS7LevelAuthoringDocument*, FString>;
SANDBOXEDITOR_API auto adopt_unbound_level_entities(ULevel& level,
                                                    AS7LevelAuthoringDocument& document) -> int32;
SANDBOXEDITOR_API auto s7_editor_config_path(AS7LevelAuthoringDocument const& document) -> FString;
SANDBOXEDITOR_API auto read_s7_editor_input(AS7LevelAuthoringDocument const& document)
    -> std::expected<levels::authoring::LevelInput, FString>;
SANDBOXEDITOR_API auto collect_s7_editor_input(ULevel const& level,
                                               AS7LevelAuthoringDocument const& document)
    -> std::expected<levels::authoring::LevelInput, FString>;
SANDBOXEDITOR_API auto export_s7_editor_config(ULevel& level, AS7LevelAuthoringDocument& document)
    -> std::expected<void, FString>;
SANDBOXEDITOR_API auto read_s7_authored_level(AS7LevelAuthoringDocument const& document)
    -> std::expected<levels::LevelDefinition, FString>;
SANDBOXEDITOR_API auto import_s7_level(ULevel& level,
                                       AS7LevelAuthoringDocument& document,
                                       levels::LevelDefinition const& definition,
                                       bool reset_export_ownership = false)
    -> std::expected<void, FString>;
}
