#pragma once

#include <ioj/levels/diagnostic_code.h>
#include <ioj/levels/diagnostics.h>
#include <ioj/levels/identifiers.h>
#include <ioj/levels/level_mission_mode.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ioj::levels {
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
    LevelId id{};
    std::string title{};
    std::string description{};
    std::optional<float> par_time_seconds{};
};

struct EntitySpawnDefinition {
    EntityId id{};
    std::string archetype{};
    TeamId team{};
    Vector3d position{};
    Rotator3d rotation{};
    double spawn_time_seconds{};
};

struct LevelCameraDefinition {
    std::vector<EntityId> target_entity_ids{};
    Vector3d offset_direction{};
    double distance{};
};

struct LevelCollisionGridDefinition {
    std::optional<Vector3d> level_size{};
    std::optional<Vector3d> cell_size{};
};

struct LevelMissionDefinition {
    LevelMissionMode mode{LevelMissionMode::Unspecified};
    std::optional<float> time_limit_seconds{};
    std::optional<std::int32_t> kill_count{};
    std::vector<EntityId> hero_entity_ids{};
    std::vector<EntityId> must_survive_entity_ids{};
    std::vector<EntityId> required_kill_entity_ids{};
};

struct LevelMissionObjectiveEvent {
    double time_seconds{};
    std::vector<EntityId> must_survive_entity_ids{};
    std::vector<EntityId> required_kill_entity_ids{};
    std::int32_t kill_target_increase{};
};

struct LevelDefinition {
    LevelMetadata metadata{};
    std::vector<LevelId> unlock_level_ids{};
    EntityId player_entity_id{};
    std::optional<LevelCameraDefinition> camera{};
    std::optional<LevelCollisionGridDefinition> collision_grid{};
    std::optional<LevelMissionDefinition> mission{};
    std::vector<LevelMissionObjectiveEvent> mission_events{};
    std::vector<TeamId> teams{};
    std::vector<EntitySpawnDefinition> entities{};
};

[[nodiscard]] auto validate_level(LevelDefinition const& definition)
    -> std::expected<void, Diagnostics>;
} // namespace ioj::levels
