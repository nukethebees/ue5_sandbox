#pragma once

#include <ioj/sim/levels/level_mission_mode.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ioj::sim::levels {
struct Vector3d {
    double x{};
    double y{};
    double z{};
};

struct Rotator3d {
    double pitch{};
    double yaw{};
    double roll{};
};

struct LevelMetadata {
    std::string id{};
    std::string title{};
    std::string description{};
    std::optional<float> par_time_seconds{};
};

struct EntitySpawnDefinition {
    std::string id{};
    std::string archetype{};
    std::string team{};
    Vector3d position{};
    Rotator3d rotation{};
    double spawn_time_seconds{};
};

struct LevelCameraDefinition {
    std::vector<std::string> target_entity_ids{};
    Vector3d offset_direction{};
    double distance{};
};

struct LevelMissionDefinition {
    LevelMissionMode mode{LevelMissionMode::Unspecified};
    std::optional<float> time_limit_seconds{};
    std::optional<std::int32_t> kill_count{};
    std::vector<std::string> hero_entity_ids{};
    std::vector<std::string> must_survive_entity_ids{};
    std::vector<std::string> required_kill_entity_ids{};
};

struct LevelMissionObjectiveEvent {
    double time_seconds{};
    std::vector<std::string> must_survive_entity_ids{};
    std::vector<std::string> required_kill_entity_ids{};
    std::int32_t kill_target_increase{};
};

struct LevelDefinition {
    LevelMetadata metadata{};
    std::vector<std::string> unlock_level_ids{};
    std::string player_entity_id{};
    std::optional<LevelCameraDefinition> camera{};
    std::optional<LevelMissionDefinition> mission{};
    std::vector<LevelMissionObjectiveEvent> mission_events{};
    std::vector<std::string> teams{};
    std::vector<EntitySpawnDefinition> entities{};
};

enum class LevelValidationErrorCode : std::uint8_t {
    MissingLevelId,
    MissingTitle,
    InvalidParTime,
    UnexpectedParTime,
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

struct LevelValidationError {
    LevelValidationErrorCode code{};
    std::string message{};
};

struct LevelValidationResult {
    std::vector<LevelValidationError> errors{};

    explicit operator bool() const noexcept { return errors.empty(); }
};

[[nodiscard]] auto validate_level(LevelDefinition const& definition) -> LevelValidationResult;
} // namespace ioj::sim::levels
