#pragma once

#include <SpaceGame/levels/LevelDefinition.h>

#include <CoreMinimal.h>

#include <expected>

class AS7LevelAuthoringDocument;
class ULevel;

namespace ml::editor {
enum class ES7LevelSyncAction : uint8 {
    Add,
    Update,
    Replace,
    Remove,
};

struct FS7LevelSyncChange {
    FLevelEntityId id{};
    ES7LevelSyncAction action{};
};

struct SANDBOXEDITOR_API FS7LevelSyncPlan {
    FLevelDefinition definition{};
    TArray<FS7LevelSyncChange> changes{};
    bool metadata_changed{};
    bool viewpoint_changed{};
    bool mission_changed{};

    [[nodiscard]] auto count(ES7LevelSyncAction action) const -> int32;
    [[nodiscard]] auto has_changes() const -> bool;
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
