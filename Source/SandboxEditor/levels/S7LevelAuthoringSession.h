#pragma once

#include <ioj/levels/authoring/scene_snapshot.h>
#include <ioj/levels/level_definition.h>

#include <CoreMinimal.h>

#include <expected>

class AS7LevelAuthoringDocument;
class ULevel;

namespace ioj::editor {
SANDBOXEDITOR_API auto read_s7_document_settings(AS7LevelAuthoringDocument const& document)
    -> std::expected<settings::Settings, FString>;
SANDBOXEDITOR_API auto s7_settings_path(ULevel const& level) -> FString;
SANDBOXEDITOR_API auto emit_s7_scene_source(levels::LevelDefinition const& definition,
                                            AS7LevelAuthoringDocument const& document)
    -> std::expected<FString, FString>;
SANDBOXEDITOR_API auto collect_s7_scene_source(ULevel const& level,
                                               AS7LevelAuthoringDocument const& document)
    -> std::expected<FString, FString>;
SANDBOXEDITOR_API auto
    collect_s7_scene_source(ULevel const& level,
                            AS7LevelAuthoringDocument const& document,
                            std::expected<levels::authoring::SceneSnapshot, FString> const& scene)
        -> std::expected<FString, FString>;

struct FS7LevelBindingRepairResult {
    int32 removed_bindings{};
    int32 removed_references{};
    int32 adopted_entities{};

    [[nodiscard]] auto has_changes() const -> bool {
        return removed_bindings != 0 || removed_references != 0 || adopted_entities != 0;
    }
};

SANDBOXEDITOR_API auto find_level_authoring_document(ULevel const& level)
    -> std::expected<AS7LevelAuthoringDocument*, FString>;
SANDBOXEDITOR_API auto create_level_authoring_document(ULevel& level)
    -> std::expected<AS7LevelAuthoringDocument*, FString>;
SANDBOXEDITOR_API auto adopt_unbound_level_entities(ULevel& level,
                                                    AS7LevelAuthoringDocument& document) -> int32;
SANDBOXEDITOR_API auto repair_s7_level_bindings(ULevel& level, AS7LevelAuthoringDocument& document)
    -> FS7LevelBindingRepairResult;
SANDBOXEDITOR_API auto collect_s7_editor_level(ULevel const& level,
                                               AS7LevelAuthoringDocument const& document)
    -> std::expected<levels::LevelDefinition, FString>;
SANDBOXEDITOR_API auto
    collect_s7_editor_level(ULevel const& level,
                            AS7LevelAuthoringDocument const& document,
                            std::expected<levels::authoring::SceneSnapshot, FString> const& scene)
        -> std::expected<levels::LevelDefinition, FString>;
// Check binding structure, but retain invalid numeric values and unresolved references for repair.
SANDBOXEDITOR_API auto collect_s7_scene(ULevel const& level,
                                        AS7LevelAuthoringDocument const& document)
    -> std::expected<levels::authoring::SceneSnapshot, FString>;
}
