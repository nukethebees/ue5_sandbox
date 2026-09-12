#pragma once

#include <SpaceGame/levels/LevelDefinition.h>
#include <SpaceGame/levels/LevelEntityResolution.h>

#include <CoreMinimal.h>

#include <expected>

class ULevel;
class USpaceGameLevelConfig;

namespace ml::editor {
struct SANDBOXEDITOR_API FS7InitialStateEntity {
    FLevelEntityId id{};
    EResolvedLevelArchetype archetype{};
    ETestTeam team{};
    FTransform transform{FTransform::Identity};
};

struct SANDBOXEDITOR_API FS7UnsupportedFeatureSummary {
    int32 scheduled_spawn_group_count{};
    int32 scheduled_entity_count{};
    int32 mission_definition_count{};
    int32 mission_event_count{};
    int32 initial_camera_count{};
    int32 unlock_criterion_count{};

    [[nodiscard]] auto is_empty() const noexcept -> bool;
    [[nodiscard]] auto format() const -> FString;
};

struct SANDBOXEDITOR_API FS7InitialStateImportPlan {
    FLevelId level_id{};
    FString level_title{};
    TArray<FS7InitialStateEntity> entities{};
    FS7UnsupportedFeatureSummary unsupported{};
};

struct SANDBOXEDITOR_API FS7InitialStateMaterialisationResult {
    int32 imported_entity_count{};
    FString error{};

    explicit operator bool() const noexcept { return error.IsEmpty(); }
};

SANDBOXEDITOR_API auto make_s7_initial_state_import_plan(FLevelDefinition const& definition)
    -> std::expected<FS7InitialStateImportPlan, FString>;
SANDBOXEDITOR_API auto materialise_s7_initial_state(ULevel& level,
                                                    USpaceGameLevelConfig& config,
                                                    FS7InitialStateImportPlan const& plan)
    -> FS7InitialStateMaterialisationResult;

SANDBOXEDITOR_API void execute_s7_initial_state_import();
}
