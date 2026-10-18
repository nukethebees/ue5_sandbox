#pragma once

#include <ioj/levels/authoring/editor_level.h>
#include <ioj/levels/authoring/sync_plan.h>

#include <CoreMinimal.h>

#include <expected>

class AS7LevelAuthoringDocument;
class ULevel;

namespace ioj::editor {
using ES7LevelSyncAction = ::ioj::levels::authoring::SyncAction;
struct SANDBOXEDITOR_API FS7LevelSyncPlan : ::ioj::levels::authoring::SyncPlan {
    ::ioj::levels::authoring::EditorLevel definition;
};

SANDBOXEDITOR_API auto make_s7_level_sync_plan(ULevel const& level,
                                               AS7LevelAuthoringDocument const& document,
                                               ::ioj::levels::authoring::EditorLevel definition)
    -> std::expected<FS7LevelSyncPlan, FString>;

SANDBOXEDITOR_API auto apply_s7_level_sync_plan(ULevel& level,
                                                AS7LevelAuthoringDocument& document,
                                                FS7LevelSyncPlan const& plan)
    -> std::expected<void, FString>;
}
