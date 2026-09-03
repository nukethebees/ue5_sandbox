#pragma once

#include <SpaceGame/levels/LevelDefinitionSoA.h>
#include <SpaceGame/levels/LevelTypes.h>

#include <CoreMinimal.h>

namespace ml {
struct SPACEGAME_API FLevelMetadata {
    FString title{};
    FString description{};
};

struct SPACEGAME_API FTeamDefinition {
    FLevelTeamId id{};
};

struct SPACEGAME_API FPlayerDefinition {
    FLevelEntityId id{};
    FEntityArchetypeId archetype{};
    FLevelTeamId team{};
    FVector position{FVector::ZeroVector};
    FRotator rotation{FRotator::ZeroRotator};
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

struct SPACEGAME_API FLevelDefinition {
    FLevelMetadata metadata{};
    FLevelEntityId player_entity_id{};
    TOptional<FLevelCameraDefinition> camera{NullOpt};
    TArray<FLevelTeamId> teams{};
    FLevelEntityTable entities{};
};

class SPACEGAME_API FLevelBuilder {
  public:
    void set_metadata(FLevelMetadata const& metadata);
    void set_player_entity(FLevelEntityId id);
    void set_player(FPlayerDefinition const& player);
    void set_camera(FLevelCameraDefinition const& camera);
    auto add_team(FTeamDefinition const& team) -> FLevelTeamId;
    auto add_entity(FEntitySpawnDefinition const& entity) -> FLevelEntityId;
    auto finish() -> FLevelDefinition;
  private:
    FLevelDefinition definition_{};
    TOptional<int32> player_row_index_{NullOpt};
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
