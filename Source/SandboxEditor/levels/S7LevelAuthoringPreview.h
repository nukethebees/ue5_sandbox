#pragma once

#include "SandboxEditor/levels/S7LevelReconciliation.h"

#include <CoreMinimal.h>
#include <UObject/WeakObjectPtr.h>

#include <expected>

class AActor;
class AS7LevelAuthoringDocument;
class ULevel;
class USpaceGameLevelConfig;

namespace ml::editor {
class FS7LevelSourceSession;

struct SANDBOXEDITOR_API FS7LevelPreviewBindingSnapshot {
    FName id{NAME_None};
    TWeakObjectPtr<AActor> actor{};
};

struct SANDBOXEDITOR_API FS7LevelAuthoringPreview {
    FS7LevelSyncPlan plan{};
    uint64 source_revision{};
    FString source_path{};
    TWeakObjectPtr<ULevel> level{};
    TWeakObjectPtr<AS7LevelAuthoringDocument> document{};
    TWeakObjectPtr<USpaceGameLevelConfig> level_config{};
    TWeakObjectPtr<UClass> level_config_class{};
    FString level_config_digest{};
    TArray<FS7LevelPreviewBindingSnapshot> bindings{};
    FString scene_digest{};
};

SANDBOXEDITOR_API auto make_s7_level_authoring_preview(ULevel& level,
                                                       AS7LevelAuthoringDocument& document,
                                                       FS7LevelSourceSession const& source_session,
                                                       FS7LevelSyncPlan plan)
    -> std::expected<FS7LevelAuthoringPreview, FString>;
SANDBOXEDITOR_API auto
    validate_s7_level_authoring_preview(ULevel& level,
                                        AS7LevelAuthoringDocument& document,
                                        FS7LevelSourceSession const& source_session,
                                        FS7LevelAuthoringPreview const& preview)
        -> std::expected<void, FString>;
SANDBOXEDITOR_API auto apply_s7_level_authoring_preview(ULevel& level,
                                                        AS7LevelAuthoringDocument& document,
                                                        FS7LevelSourceSession const& source_session,
                                                        FS7LevelAuthoringPreview const& preview)
    -> std::expected<void, FString>;
}
