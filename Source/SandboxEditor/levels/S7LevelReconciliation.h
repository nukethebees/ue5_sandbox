#pragma once

#include <ioj/levels/authoring/sync_plan.h>
#include <ioj/levels/level_definition.h>

#include <CoreMinimal.h>

#include <expected>

class AS7LevelAuthoringDocument;
class ULevel;

namespace ioj::editor {
using ES7LevelSyncAction = levels::authoring::SyncAction;
struct SANDBOXEDITOR_API FS7LevelSyncPlan : levels::authoring::SyncPlan {
    levels::LevelDefinition definition;
};

SANDBOXEDITOR_API auto make_s7_level_sync_plan(ULevel const& level,
                                               AS7LevelAuthoringDocument const& document,
                                               levels::LevelDefinition definition)
    -> std::expected<FS7LevelSyncPlan, FString>;

SANDBOXEDITOR_API auto apply_s7_level_sync_plan(ULevel& level,
                                                AS7LevelAuthoringDocument& document,
                                                FS7LevelSyncPlan const& plan)
    -> std::expected<void, FString>;
}
