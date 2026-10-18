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

enum class ES7LevelPreviewError { Stale, Execution };
struct FS7LevelPreviewError {
    ES7LevelPreviewError code{};
    FString message{};
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
    TArray<TWeakObjectPtr<AActor>> actor_identities{};
    ::ioj::levels::authoring::SceneSnapshot scene{};
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
        -> std::expected<void, FS7LevelPreviewError>;
SANDBOXEDITOR_API auto apply_s7_level_authoring_preview(ULevel& level,
                                                        AS7LevelAuthoringDocument& document,
                                                        FS7LevelSourceSession const& source_session,
                                                        FS7LevelAuthoringPreview const& preview)
    -> std::expected<void, FS7LevelPreviewError>;
}
