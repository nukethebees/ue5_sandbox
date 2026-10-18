#pragma once

#include <ioj/levels/authoring/scene.h>
#include <SpaceGame/levels/LevelDefinition.h>

#include <CoreMinimal.h>

#include <expected>

class AS7LevelAuthoringDocument;
class ULevel;

namespace ml::editor {
using ES7LevelSyncAction = ::ioj::levels::authoring::SyncAction;
struct SANDBOXEDITOR_API FS7LevelSyncPlan : ::ioj::levels::authoring::SyncPlan {
    FLevelDefinition definition{};
};

SANDBOXEDITOR_API auto make_s7_level_sync_plan(ULevel const& level,
                                               AS7LevelAuthoringDocument const& document,
                                               FLevelDefinition const& definition)
    -> std::expected<FS7LevelSyncPlan, FString>;
SANDBOXEDITOR_API auto apply_s7_level_sync_plan(ULevel& level,
                                                AS7LevelAuthoringDocument& document,
                                                FS7LevelSyncPlan const& plan)
    -> std::expected<void, FString>;
}
