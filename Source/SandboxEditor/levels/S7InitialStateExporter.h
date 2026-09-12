#pragma once

#include <SpaceGame/levels/LevelDefinition.h>

#include <CoreMinimal.h>

#include <expected>

class ULevel;

namespace ml::editor {
struct SANDBOXEDITOR_API FS7InitialStateExportWarnings {
    TMap<FName, int32> unsupported_actor_classes{};
    int32 invalid_team_actor_count{};
    int32 invalid_transform_actor_count{};
    int32 ignored_scale_actor_count{};
    int32 flattened_attachment_actor_count{};
    int32 ignored_property_override_count{};

    [[nodiscard]] auto is_empty() const noexcept -> bool;
    [[nodiscard]] auto format() const -> FString;
};

struct SANDBOXEDITOR_API FS7InitialStateExportPlan {
    FLevelDefinition definition{};
    FS7InitialStateExportWarnings warnings{};
};

SANDBOXEDITOR_API auto collect_s7_initial_state(ULevel const& level, FLevelMetadata const& metadata)
    -> std::expected<FS7InitialStateExportPlan, FString>;
SANDBOXEDITOR_API void execute_s7_initial_state_export();
}
