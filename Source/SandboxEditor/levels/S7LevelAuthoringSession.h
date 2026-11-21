#pragma once

#include "SandboxEditor/levels/S7AuthoringResult.h"
#include <ioj/levels/authoring/level_inputs.h>
#include <ioj/levels/level_definition.h>

#include <CoreMinimal.h>

class AS7LevelAuthoringDocument;
class ULevel;

namespace ioj::editor {
SANDBOXEDITOR_API auto find_level_authoring_document(ULevel const& level)
    -> S7AuthoringResult<AS7LevelAuthoringDocument*>;
enum class MissionSetup { Create, Link };
SANDBOXEDITOR_API auto
    set_up_s7_mission(ULevel& level, MissionSetup operation, FString source_path = {})
        -> S7AuthoringResult<AS7LevelAuthoringDocument*>;
SANDBOXEDITOR_API auto s7_mission_path(AS7LevelAuthoringDocument const& document) -> FString;
SANDBOXEDITOR_API auto check_s7_export(ULevel const& level,
                                       AS7LevelAuthoringDocument const& document)
    -> S7AuthoringResult<void>;
SANDBOXEDITOR_API auto s7_editor_config_path(AS7LevelAuthoringDocument const& document) -> FString;
SANDBOXEDITOR_API auto read_s7_editor_input(AS7LevelAuthoringDocument const& document)
    -> S7AuthoringResult<levels::authoring::LevelInput>;
SANDBOXEDITOR_API auto s7_scene_needs_export(AS7LevelAuthoringDocument const& document)
    -> S7AuthoringResult<bool>;
SANDBOXEDITOR_API auto export_s7_editor_config(ULevel& level, AS7LevelAuthoringDocument& document)
    -> S7AuthoringResult<void>;
SANDBOXEDITOR_API auto read_s7_authored_level(AS7LevelAuthoringDocument const& document)
    -> S7AuthoringResult<levels::LevelDefinition>;
SANDBOXEDITOR_API auto import_s7_level(ULevel& level, AS7LevelAuthoringDocument& document)
    -> S7AuthoringResult<void>;
}
