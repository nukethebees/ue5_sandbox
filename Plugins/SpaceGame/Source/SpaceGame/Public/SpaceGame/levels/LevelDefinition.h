#pragma once

#include <SpaceGame/levels/LevelDefinitionSoA.h>
#include <SpaceGame/levels/LevelTypes.h>

#include <CoreMinimal.h>

namespace ml {
struct SPACEGAME_API FLevelMetadata {
    FString title{};
    FString description{};
};

struct SPACEGAME_API FEntitySpawnDefinition {
    FLevelEntityId id{};
    FEntityArchetypeId archetype{};
    FLevelTeamId team{};
    FVector position{FVector::ZeroVector};
    FRotator rotation{FRotator::ZeroRotator};
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

struct SPACEGAME_API FLevelDefinition {
    FLevelMetadata metadata{};
    FLevelEntityId player_entity_id{};
    TOptional<FLevelCameraDefinition> camera{NullOpt};
    TOptional<FLevelMissionDefinition> mission{NullOpt};
    TArray<FLevelTeamId> teams{};
    FLevelEntityTable entities{};
};

class SPACEGAME_API FLevelBuilder {
  public:
    void set_metadata(FLevelMetadata const& metadata);
    void set_player_entity(FLevelEntityId id);
    void set_camera(FLevelCameraDefinition const& camera);
    void set_mission(FLevelMissionDefinition const& mission);
    auto add_team(FLevelTeamId team) -> FLevelTeamId;
    auto add_entity(FEntitySpawnDefinition const& entity) -> FLevelEntityId;
    auto finish() -> FLevelDefinition;
  private:
    FLevelDefinition definition_{};
};

enum class ELevelValidationErrorCode : uint8 {
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
