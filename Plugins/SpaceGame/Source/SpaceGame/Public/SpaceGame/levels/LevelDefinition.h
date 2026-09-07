#pragma once

#include <SpaceGame/levels/LevelDefinitionSoA.h>
#include <SpaceGame/levels/LevelTypes.h>

#include <CoreMinimal.h>
#include <Misc/TVariant.h>

namespace ml {
struct SPACEGAME_API FLevelMetadata {
    FLevelId id{};
    FString title{};
    FString description{};
};

struct SPACEGAME_API FEntitySpawnDefinition {
    FLevelEntityId id{};
    FEntityArchetypeId archetype{};
    FLevelTeamId team{};
    FVector position{FVector::ZeroVector};
    FRotator rotation{FRotator::ZeroRotator};
    double spawn_time_seconds{};
};

struct SPACEGAME_API FLevelCameraDefinition {
    TArray<FLevelEntityId> target_entity_ids{};
    FVector offset_direction{FVector::ZeroVector};
    double distance{0.0};
};

enum class ELevelMissionMode : uint8 {
    Unspecified,
    SurviveTime,
    KillEnemies,
    KillEnemiesWithinTime,
};

struct SPACEGAME_API FLevelMissionDefinition {
    ELevelMissionMode mode{ELevelMissionMode::Unspecified};
    TOptional<float> time_limit_seconds{NullOpt};
    TOptional<int32> kill_count{NullOpt};
    TArray<FLevelEntityId> hero_entity_ids{};
    TArray<FLevelEntityId> must_survive_entity_ids{};
    TArray<FLevelEntityId> required_kill_entity_ids{};
};

struct SPACEGAME_API FLevelCompletedUnlockCriterion {
    FLevelId level_id{};

    auto operator==(FLevelCompletedUnlockCriterion const&) const -> bool = default;
};

using FLevelUnlockCriterion = TVariant<FLevelCompletedUnlockCriterion>;

struct SPACEGAME_API FLevelMissionObjectiveEvent {
    double time_seconds{};
    TArray<FLevelEntityId> must_survive_entity_ids{};
    TArray<FLevelEntityId> required_kill_entity_ids{};
    int32 kill_target_increase{};
};

struct SPACEGAME_API FLevelDefinition {
    FLevelMetadata metadata{};
    TArray<FLevelUnlockCriterion> unlock_criteria{};
    FLevelEntityId player_entity_id{};
    TOptional<FLevelCameraDefinition> camera{NullOpt};
    TOptional<FLevelMissionDefinition> mission{NullOpt};
    TArray<FLevelMissionObjectiveEvent> mission_events{};
    TArray<FLevelTeamId> teams{};
    FLevelEntityTable entities{};
};

class SPACEGAME_API FLevelBuilder {
  public:
    void set_metadata(FLevelMetadata const& metadata);
    void set_player_entity(FLevelEntityId id);
    void set_camera(FLevelCameraDefinition const& camera);
    void set_mission(FLevelMissionDefinition const& mission);
    void add_unlock_criterion(FLevelUnlockCriterion criterion);
    void add_mission_event(FLevelMissionObjectiveEvent const& event);
    auto add_team(FLevelTeamId team) -> FLevelTeamId;
    auto add_entity(FEntitySpawnDefinition const& entity) -> FLevelEntityId;
    auto finish() -> FLevelDefinition;
  private:
    FLevelDefinition definition_{};
};

enum class ELevelValidationErrorCode : uint8 {
    MissingLevelId,
    MissingTitle,
    MissingViewpoint,
    ConflictingViewpoints,
    PlayerEntityNotFound,
    MismatchedEntityColumns,
    EmptyTeamId,
    DuplicateTeamId,
    UnsupportedTeamId,
    UnknownTeamReference,
    EmptyArchetypeId,
    UnsupportedArchetype,
    ArchetypeRoleMismatch,
    DuplicateEntityId,
    InvalidPlacement,
    InvalidSpawnTime,
    DelayedPlayerSpawn,
    MissingCameraTarget,
    DuplicateCameraTarget,
    CameraTargetNotFound,
    InvalidCameraDistance,
    InvalidCameraOffsetDirection,
    MissingMissionMode,
    UnsupportedMissionMode,
    InvalidMissionTimeLimit,
    UnexpectedMissionTimeLimit,
    InvalidMissionKillCount,
    UnexpectedMissionKillCount,
    MissingMissionHeroes,
    MissingMissionSurvivors,
    MissionEntityNotFound,
    DuplicateMissionEntityReference,
    ConflictingMissionEntityRoles,
    AmbiguousAutomaticKillTeams,
    MissingUnlockLevelId,
    DuplicateUnlockCriterion,
    SelfUnlockDependency,
    UnexpectedMissionEvent,
    InvalidMissionEventTime,
    InvalidMissionKillIncrease,
    MissionEventBeforeEntitySpawn,
};

struct SPACEGAME_API FLevelValidationError {
    ELevelValidationErrorCode code{};
    FString message{};
};

struct SPACEGAME_API FLevelValidationResult {
    TArray<FLevelValidationError> errors{};

    explicit operator bool() const noexcept { return errors.IsEmpty(); }
};

SPACEGAME_API auto validate_level(FLevelDefinition const& definition) -> FLevelValidationResult;
}
