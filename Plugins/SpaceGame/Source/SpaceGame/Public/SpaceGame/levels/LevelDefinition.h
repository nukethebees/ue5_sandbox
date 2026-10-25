#pragma once
#include <ioj/levels/level_definition.h>
#include <ioj/settings/settings.h>
#include <ioj/sim/levels/level_capital_spawn_events.h>
#include <ioj/sim/levels/level_spawn_groups.h>
#include <ioj/sim/levels/level_spinner_spawn_events.h>
#include <ioj/sim/levels/level_turret_spawn_events.h>
#include <SpaceGameSimulation/levels/LevelTypes.h>

#include <CoreMinimal.h>
#include <Misc/TVariant.h>

namespace ioj {
struct SPACEGAME_API FLevelMetadata {
    FLevelId id{};
    FString title{};
    FString description{};
    TOptional<float> par_time_seconds{NullOpt};
};

struct SPACEGAME_API FEntitySpawnDefinition {
    FLevelEntityId id{};
    FEntityArchetypeId archetype{};
    Team team{};
    FVector position{FVector::ZeroVector};
    FRotator rotation{FRotator::ZeroRotator};
    double spawn_time_seconds{};
};

struct SPACEGAME_API FLevelCameraDefinition {
    TArray<FLevelEntityId> target_entity_ids{};
    FVector offset_direction{FVector::ZeroVector};
    double distance{0.0};
};

struct SPACEGAME_API FLevelCompletedUnlockCriterion {
    FLevelId level_id{};

    auto operator==(FLevelCompletedUnlockCriterion const&) const -> bool = default;
};

using FLevelUnlockCriterion = TVariant<FLevelCompletedUnlockCriterion>;

// Hold mutable Unreal authoring data until to_native constructs a trusted LevelDefinition.
struct SPACEGAME_API FLevelDraft {
    FLevelMetadata metadata{};
    TArray<FLevelUnlockCriterion> unlock_criteria{};
    FLevelEntityId player_entity_id{};
    TOptional<FLevelCameraDefinition> camera{NullOpt};
    settings::Settings settings{};
    TOptional<levels::MissionDefinition> mission{NullOpt};
    TArray<FEntitySpawnDefinition> entities{};
};

class SPACEGAME_API FLevelBuilder {
  public:
    void set_metadata(FLevelMetadata const& metadata);
    void set_player_entity(FLevelEntityId id);
    void set_camera(FLevelCameraDefinition const& camera);
    void set_settings(settings::Settings const& settings);
    void set_mission(levels::MissionDefinition const& mission);
    void add_unlock_criterion(FLevelUnlockCriterion criterion);
    auto add_entity(FEntitySpawnDefinition const& entity) -> FLevelEntityId;
    auto finish() -> FLevelDraft;
  private:
    FLevelDraft definition_{};
};

using ELevelValidationErrorCode = levels::DiagnosticCode;

struct SPACEGAME_API FLevelValidationError {
    ELevelValidationErrorCode code{};
    FString message{};
};

struct SPACEGAME_API FLevelValidationResult {
    TArray<FLevelValidationError> errors{};

    explicit operator bool() const noexcept { return errors.IsEmpty(); }
};

SPACEGAME_API auto validate_level(FLevelDraft const& definition) -> FLevelValidationResult;
}
