#pragma once

#include <ioj/levels/level_definition.h>

#include <unordered_map>
#include <unordered_set>

namespace ioj::levels::detail {
using IdSet = std::unordered_set<EntityId>;
struct EntityValidationState {
    IdSet ids{};
    std::unordered_map<EntityId, TeamId> teams_by_id{};
    std::unordered_map<EntityId, double> spawn_times_by_id{};
    bool player_found{false};
};

inline void add_error(Diagnostics& result,
                      std::string path,
                      DiagnosticCode const code,
                      std::string message) {
    result.push_back({code, std::move(path), std::move(message)});
}

auto validate_entities(LevelDefinition const& definition,
                       std::unordered_set<TeamId> const& declared_teams,
                       Diagnostics& result) -> EntityValidationState;
void validate_mission(LevelMissionDefinition const& mission,
                      EntityValidationState const& entities,
                      Diagnostics& result);
void validate_mission_events(LevelDefinition const& definition,
                             EntityValidationState const& entities,
                             Diagnostics& result);
void validate_camera(LevelCameraDefinition const& camera,
                     EntityValidationState const& entities,
                     Diagnostics& result);
void validate_collision_grid(LevelCollisionGridDefinition const& grid, Diagnostics& result);
}
