#include <ioj/levels/level_definition.h>

#include <ioj/ascii.h>
#include <ioj/grid_dimensions.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace ioj::levels {
namespace {
using IdSet = std::unordered_set<EntityId>;

void add_error(Diagnostics& result,
               std::string path,
               DiagnosticCode const code,
               std::string message) {
    result.push_back({code, std::move(path), std::move(message)});
}

auto owner(EntitySpawnDefinition const& entity, std::size_t const index) -> std::string {
    return entity.id.empty() ? "Entity at row " + std::to_string(index)
                             : "Entity '" + entity.id.value + "'";
}

auto supported_team(TeamId const& id) -> bool {
    return id.value == "white" || id.value == "red" || id.value == "green" || id.value == "blue" ||
           id.value == "orange" || id.value == "yellow";
}

enum class Archetype { Player, Capital, Turret, Unsupported };

auto resolve_archetype(std::string_view const id) -> Archetype {
    if (id == "player-fighter") {
        return Archetype::Player;
    }
    if (id == "capital-ship") {
        return Archetype::Capital;
    }
    if (id == "static-turret") {
        return Archetype::Turret;
    }
    return Archetype::Unsupported;
}

struct EntityValidationState {
    IdSet ids{};
    std::unordered_map<EntityId, TeamId> teams_by_id{};
    std::unordered_map<EntityId, double> spawn_times_by_id{};
    bool player_found{false};
};

auto validate_entities(LevelDefinition const& definition,
                       std::unordered_set<TeamId> const& declared_teams,
                       Diagnostics& result) -> EntityValidationState {
    EntityValidationState state;
    state.ids.reserve(definition.entities.size());
    state.teams_by_id.reserve(definition.entities.size());
    state.spawn_times_by_id.reserve(definition.entities.size());

    auto const entity_count{definition.entities.size()};
    for (std::size_t index{}; index < entity_count; ++index) {
        auto const& entity{definition.entities[index]};
        auto const entity_owner{owner(entity, index)};
        auto const error_begin{result.size()};
        auto const is_player{!definition.player_entity_id.empty() &&
                             entity.id == definition.player_entity_id};

        if (!entity.id.empty()) {
            if (!state.ids.insert(entity.id).second) {
                add_error(result,
                          "level.entities.id",
                          DiagnosticCode::DuplicateEntityId,
                          entity_owner + " duplicates authored entity id '" + entity.id.value +
                              "'");
            } else {
                state.teams_by_id.emplace(entity.id, entity.team);
                state.spawn_times_by_id.emplace(entity.id, entity.spawn_time_seconds);
            }
        }

        state.player_found = state.player_found || is_player;
        if (!declared_teams.contains(entity.team)) {
            add_error(result,
                      "level.entities.team",
                      DiagnosticCode::UnknownTeamReference,
                      entity_owner + " references undeclared team '" + entity.team.value + "'");
        }

        if (entity.archetype.empty()) {
            add_error(result,
                      "level.entities.archetype",
                      DiagnosticCode::EmptyArchetypeId,
                      entity_owner + " has an empty archetype id");
        } else {
            auto const archetype{resolve_archetype(entity.archetype)};
            if (archetype == Archetype::Unsupported) {
                add_error(result,
                          "level.entities.archetype",
                          DiagnosticCode::UnsupportedArchetype,
                          entity_owner + " uses unsupported archetype '" + entity.archetype + "'");
            } else if (is_player != (archetype == Archetype::Player)) {
                add_error(result,
                          "level.entities.archetype",
                          DiagnosticCode::ArchetypeRoleMismatch,
                          is_player ? "Player cannot use archetype '" + entity.archetype + "'"
                                    : entity_owner + " cannot use the player archetype");
            }
        }

        auto const placement_is_finite{
            std::isfinite(entity.position.x) && std::isfinite(entity.position.y) &&
            std::isfinite(entity.position.z) && std::isfinite(entity.rotation.pitch) &&
            std::isfinite(entity.rotation.yaw) && std::isfinite(entity.rotation.roll)};
        if (!placement_is_finite) {
            add_error(result,
                      "level.entities.position",
                      DiagnosticCode::InvalidPlacement,
                      entity_owner + " has a non-finite position or rotation");
        }
        if (!std::isfinite(entity.spawn_time_seconds) || entity.spawn_time_seconds < 0.0) {
            add_error(result,
                      "level.entities.spawn-at",
                      DiagnosticCode::InvalidSpawnTime,
                      entity_owner + " has an invalid spawn time");
        }
        if (is_player && entity.spawn_time_seconds != 0.0) {
            add_error(result,
                      "level.entities.spawn-at",
                      DiagnosticCode::DelayedPlayerSpawn,
                      "The player entity must spawn at time zero");
        }
        auto const error_end{result.size()};
        for (auto error_index{error_begin}; error_index < error_end; ++error_index) {
            auto& path{result[error_index].node_path};
            path.insert(std::string_view{"level.entities"}.size(), std::format("[{}]", index));
        }
    }

    if (!definition.player_entity_id.empty() && !state.player_found) {
        add_error(result,
                  "level.player",
                  DiagnosticCode::PlayerEntityNotFound,
                  "Player entity '" + definition.player_entity_id.value + "' is not declared");
    }
    return state;
}

auto validate_references(std::vector<EntityId> const& references,
                         std::string_view const role,
                         IdSet const& entity_ids,
                         Diagnostics& result) -> IdSet {
    IdSet validated;
    validated.reserve(references.size());
    for (auto const& id : references) {
        if (id.empty() || !entity_ids.contains(id)) {
            add_error(result,
                      "level.mission",
                      DiagnosticCode::MissionEntityNotFound,
                      "Mission " + std::string{role} + " entity '" + id.value +
                          "' is not declared");
        } else if (!validated.insert(id).second) {
            add_error(result,
                      "level.mission",
                      DiagnosticCode::DuplicateMissionEntityReference,
                      "Mission " + std::string{role} + " entity '" + id.value + "' is duplicated");
        }
    }
    return validated;
}

void validate_initial_spawns(IdSet const& ids,
                             EntityValidationState const& entities,
                             Diagnostics& result) {
    for (auto const& id : ids) {
        auto const found{entities.spawn_times_by_id.find(id)};
        if (found != entities.spawn_times_by_id.end() && found->second > 0.0) {
            add_error(result,
                      "level.mission",
                      DiagnosticCode::MissionEventBeforeEntitySpawn,
                      "Initial mission objective entity '" + id.value +
                          "' does not spawn at time zero");
        }
    }
}

void validate_mission(LevelMissionDefinition const& mission,
                      EntityValidationState const& entities,
                      Diagnostics& result) {
    auto requires_time_limit{false};
    auto uses_kill_count{false};
    switch (mission.mode) {
        case LevelMissionMode::Unspecified:
            add_error(result,
                      "level.mission.mode",
                      DiagnosticCode::MissingMissionMode,
                      "Mission definition has no mode");
            break;
        case LevelMissionMode::SurviveTime:
            requires_time_limit = true;
            if (mission.must_survive_entity_ids.empty()) {
                add_error(result,
                          "level.mission.must-survive",
                          DiagnosticCode::MissingMissionSurvivors,
                          "Survive-time mission has no entities that must survive");
            }
            break;
        case LevelMissionMode::KillEnemies:
            uses_kill_count = true;
            break;
        case LevelMissionMode::KillEnemiesWithinTime:
            requires_time_limit = true;
            uses_kill_count = true;
            break;
        default:
            add_error(result,
                      "level.mission.mode",
                      DiagnosticCode::UnsupportedMissionMode,
                      "Mission definition uses an unsupported mode");
            break;
    }

    if (requires_time_limit) {
        if (!mission.time_limit_seconds || !std::isfinite(*mission.time_limit_seconds) ||
            *mission.time_limit_seconds <= 0.0f) {
            add_error(result,
                      "level.mission.time-limit",
                      DiagnosticCode::InvalidMissionTimeLimit,
                      "Mission time limit must be finite and greater than zero");
        }
    } else if (mission.time_limit_seconds) {
        add_error(result,
                  "level.mission.time-limit",
                  DiagnosticCode::UnexpectedMissionTimeLimit,
                  "Untimed mission cannot define a time limit");
    }

    if (uses_kill_count) {
        if (mission.hero_entity_ids.empty()) {
            add_error(result,
                      "level.mission.heroes",
                      DiagnosticCode::MissingMissionHeroes,
                      "Kill mission has no hero entities");
        }
        if (mission.kill_count && *mission.kill_count <= 0) {
            add_error(result,
                      "level.mission.kill-count",
                      DiagnosticCode::InvalidMissionKillCount,
                      "Mission kill count must be greater than zero when specified");
        }
    } else if (mission.kill_count) {
        add_error(result,
                  "level.mission.kill-count",
                  DiagnosticCode::UnexpectedMissionKillCount,
                  "Survive-time mission cannot define a kill count");
    }

    auto const heroes{validate_references(mission.hero_entity_ids, "hero", entities.ids, result)};
    auto const survivors{
        validate_references(mission.must_survive_entity_ids, "must-survive", entities.ids, result)};
    auto const required{validate_references(
        mission.required_kill_entity_ids, "required-kill", entities.ids, result)};
    validate_initial_spawns(heroes, entities, result);
    validate_initial_spawns(survivors, entities, result);
    validate_initial_spawns(required, entities, result);

    for (auto const& id : required) {
        if (heroes.contains(id) || survivors.contains(id)) {
            add_error(result,
                      "level.mission",
                      DiagnosticCode::ConflictingMissionEntityRoles,
                      "Mission entity '" + id.value +
                          "' cannot be both required to kill and a hero or must-survive entity");
        }
    }

    if (uses_kill_count && !mission.kill_count && !heroes.empty()) {
        std::optional<TeamId> hero_team;
        for (auto const& id : heroes) {
            auto const found{entities.teams_by_id.find(id)};
            if (found == entities.teams_by_id.end()) {
                continue;
            }
            if (!hero_team) {
                hero_team = found->second;
            } else if (*hero_team != found->second) {
                add_error(result,
                          "level.mission.heroes",
                          DiagnosticCode::AmbiguousAutomaticKillTeams,
                          "Automatic kill count requires all hero entities to share a team");
                break;
            }
        }
    }
}

void validate_mission_events(LevelDefinition const& definition,
                             EntityValidationState const& entities,
                             Diagnostics& result) {
    if (definition.mission_events.empty()) {
        return;
    }
    if (!definition.mission) {
        add_error(result,
                  "level.mission-events",
                  DiagnosticCode::UnexpectedMissionEvent,
                  "Mission objective events require a mission definition");
        return;
    }

    auto const& mission{*definition.mission};
    auto must_survive{
        validate_references(mission.must_survive_entity_ids, "must-survive", entities.ids, result)};
    auto required{validate_references(
        mission.required_kill_entity_ids, "required-kill", entities.ids, result)};
    auto const event_count{definition.mission_events.size()};
    for (std::size_t event_index{}; event_index < event_count; ++event_index) {
        auto const& event{definition.mission_events[event_index]};
        auto const error_begin{result.size()};
        if (!std::isfinite(event.time_seconds) || event.time_seconds < 0.0) {
            add_error(result,
                      "level.mission-events.at",
                      DiagnosticCode::InvalidMissionEventTime,
                      "Mission objective event time must be finite and non-negative");
        }
        if (event.kill_target_increase < 0 ||
            (event.kill_target_increase > 0 && !mission.kill_count)) {
            add_error(result,
                      "level.mission-events.increase-kill-count",
                      DiagnosticCode::InvalidMissionKillIncrease,
                      "Mission kill target increases require an explicit kill count and must be "
                      "non-negative");
        }
        if (mission.time_limit_seconds && event.time_seconds > *mission.time_limit_seconds) {
            add_error(result,
                      "level.mission-events.at",
                      DiagnosticCode::InvalidMissionEventTime,
                      "Mission objective event occurs after the mission time limit");
        }

        auto validate_event = [&](std::vector<EntityId> const& references,
                                  IdSet& same_role,
                                  IdSet const& conflicting_role,
                                  std::string_view const role) {
            for (auto const& id : references) {
                auto const spawn{entities.spawn_times_by_id.find(id)};
                if (spawn == entities.spawn_times_by_id.end()) {
                    add_error(result,
                              "level.mission",
                              DiagnosticCode::MissionEntityNotFound,
                              "Mission event " + std::string{role} + " entity '" + id.value +
                                  "' is not declared");
                    continue;
                }
                if (spawn->second > event.time_seconds) {
                    add_error(result,
                              "level.mission",
                              DiagnosticCode::MissionEventBeforeEntitySpawn,
                              "Mission event references entity '" + id.value +
                                  "' before it spawns");
                }
                if (same_role.contains(id)) {
                    add_error(result,
                              "level.mission",
                              DiagnosticCode::DuplicateMissionEntityReference,
                              "Mission event " + std::string{role} + " entity '" + id.value +
                                  "' is duplicated");
                } else if (conflicting_role.contains(id)) {
                    add_error(result,
                              "level.mission",
                              DiagnosticCode::ConflictingMissionEntityRoles,
                              "Mission entity '" + id.value + "' has conflicting roles");
                } else {
                    same_role.insert(id);
                }
            }
        };
        validate_event(event.must_survive_entity_ids, must_survive, required, "must-survive");
        validate_event(event.required_kill_entity_ids, required, must_survive, "required-kill");
        auto const error_end{result.size()};
        for (auto index{error_begin}; index < error_end; ++index) {
            auto& path{result[index].node_path};
            auto const suffix{path.starts_with("level.mission-events") ? path.substr(20)
                                                                       : std::string{}};
            path = std::format("level.mission-events[{}]{}", event_index, suffix);
        }
    }
}
} // namespace

auto validate_level(LevelDefinition const& definition) -> std::expected<void, Diagnostics> {
    Diagnostics result;
    if (definition.metadata.id.empty()) {
        add_error(result,
                  "level.id",
                  DiagnosticCode::MissingLevelId,
                  "Level definition has no stable id");
    }
    if (ioj::blank(definition.metadata.title)) {
        add_error(
            result, "level.title", DiagnosticCode::MissingTitle, "Level definition has no title");
    }
    if (definition.metadata.par_time_seconds) {
        auto const par_time{*definition.metadata.par_time_seconds};
        if (!std::isfinite(par_time) || par_time <= 0.0f) {
            add_error(result,
                      "level.par-time",
                      DiagnosticCode::InvalidParTime,
                      "Level par time must be finite and greater than zero");
        }
        if (definition.player_entity_id.empty() || !definition.mission) {
            add_error(result,
                      "level.par-time",
                      DiagnosticCode::UnexpectedParTime,
                      "Level par time requires a player-controlled mission");
        }
    }
    if (definition.collision_grid) {
        auto const& grid{*definition.collision_grid};
        auto const valid_level_size{
            !grid.level_size ||
            (std::isfinite(grid.level_size->x) && std::isfinite(grid.level_size->y) &&
             std::isfinite(grid.level_size->z) && grid.level_size->x > 0.0 &&
             grid.level_size->y > 0.0 && grid.level_size->z > 0.0)};
        if (grid.level_size && !valid_level_size) {
            add_error(result,
                      "level.collision-grid.level-size",
                      DiagnosticCode::InvalidLevelSize,
                      "Collision-grid level size must be finite and greater than zero");
        }
        auto const valid_cell_size{!grid.cell_size ||
                                   (std::isfinite(grid.cell_size->x) &&
                                    std::isfinite(grid.cell_size->y) &&
                                    std::isfinite(grid.cell_size->z) && grid.cell_size->x > 0.0 &&
                                    grid.cell_size->y > 0.0 && grid.cell_size->z > 0.0)};
        if (grid.cell_size && !valid_cell_size) {
            add_error(result,
                      "level.collision-grid.cell-size",
                      DiagnosticCode::InvalidGridCellSize,
                      "Collision-grid cell size must be finite and greater than zero");
        }
        if (grid.level_size && grid.cell_size && valid_level_size && valid_cell_size) {
            auto const x{ioj::grid_axis_count(static_cast<float>(grid.level_size->x),
                                              static_cast<float>(grid.cell_size->x))};
            auto const y{ioj::grid_axis_count(static_cast<float>(grid.level_size->y),
                                              static_cast<float>(grid.cell_size->y))};
            auto const z{ioj::grid_axis_count(static_cast<float>(grid.level_size->z),
                                              static_cast<float>(grid.cell_size->z))};
            if (!ioj::grid_cell_count_fits(x, y, z)) {
                add_error(result,
                          "level.collision-grid",
                          DiagnosticCode::InvalidGridDimensions,
                          "Collision-grid dimensions and cell count must fit in 32-bit integers");
            }
        }
    }

    std::unordered_set<LevelId> unlock_ids;
    for (auto const& id : definition.unlock_level_ids) {
        if (id.empty()) {
            add_error(result,
                      "level.unlock",
                      DiagnosticCode::MissingUnlockLevelId,
                      "Level-completed unlock criterion has no level id");
            continue;
        }
        if (id == definition.metadata.id) {
            add_error(result,
                      "level.unlock",
                      DiagnosticCode::SelfUnlockDependency,
                      "Level '" + id.value + "' requires itself to be completed");
        }
        if (!unlock_ids.insert(id).second) {
            add_error(result,
                      "level.unlock",
                      DiagnosticCode::DuplicateUnlockCriterion,
                      "Level-completed criterion for '" + id.value + "' is duplicated");
        }
    }

    std::unordered_set<TeamId> teams;
    teams.reserve(definition.teams.size());
    auto const team_count{definition.teams.size()};
    for (std::size_t index{}; index < team_count; ++index) {
        auto const& team{definition.teams[index]};
        if (team.empty()) {
            add_error(result,
                      "level.teams",
                      DiagnosticCode::EmptyTeamId,
                      "Team " + std::to_string(index) + " has an empty id");
            continue;
        }
        if (!teams.insert(team).second) {
            add_error(result,
                      "level.teams",
                      DiagnosticCode::DuplicateTeamId,
                      "Team " + std::to_string(index) + " duplicates team id '" + team.value + "'");
            continue;
        }
        if (!supported_team(team)) {
            add_error(result,
                      "level.teams",
                      DiagnosticCode::UnsupportedTeamId,
                      "Team " + std::to_string(index) + " uses unsupported team id '" + team.value +
                          "'");
        }
    }

    auto const has_player{!definition.player_entity_id.empty()};
    auto const has_camera{definition.camera.has_value()};
    if (!has_player && !has_camera) {
        add_error(result,
                  "level",
                  DiagnosticCode::MissingViewpoint,
                  "Level definition has neither a player nor an initial camera");
    } else if (has_player && has_camera) {
        add_error(result,
                  "level",
                  DiagnosticCode::ConflictingViewpoints,
                  "Level definition cannot have both a player and an initial camera");
    }

    auto const entities{validate_entities(definition, teams, result)};
    if (definition.camera) {
        auto const& camera{*definition.camera};
        if (camera.target_entity_ids.empty()) {
            add_error(result,
                      "level.camera.look-at",
                      DiagnosticCode::MissingCameraTarget,
                      "Initial camera has no target entities");
        }
        IdSet targets;
        auto const target_count{camera.target_entity_ids.size()};
        for (std::size_t index{}; index < target_count; ++index) {
            auto const& id{camera.target_entity_ids[index]};
            if (!targets.insert(id).second) {
                add_error(result,
                          "level.camera.look-at",
                          DiagnosticCode::DuplicateCameraTarget,
                          "Initial camera target " + std::to_string(index) +
                              " duplicates entity '" + id.value + "'");
                continue;
            }
            if (id.empty() || !entities.ids.contains(id)) {
                add_error(result,
                          "level.camera.look-at",
                          DiagnosticCode::CameraTargetNotFound,
                          "Initial camera target '" + id.value + "' is not declared");
            }
        }
        if (!std::isfinite(camera.distance) || camera.distance <= 0.0) {
            add_error(result,
                      "level.camera.distance",
                      DiagnosticCode::InvalidCameraDistance,
                      "Initial camera distance must be finite and greater than zero");
        }
        auto const& direction{camera.offset_direction};
        auto const finite{std::isfinite(direction.x) && std::isfinite(direction.y) &&
                          std::isfinite(direction.z)};
        auto const nearly_zero{std::abs(direction.x) <= 1.0e-4 && std::abs(direction.y) <= 1.0e-4 &&
                               std::abs(direction.z) <= 1.0e-4};
        if (!finite || nearly_zero) {
            add_error(result,
                      "level.camera.offset-direction",
                      DiagnosticCode::InvalidCameraOffsetDirection,
                      "Initial camera offset direction must be finite and non-zero");
        }
    }
    if (definition.mission) {
        validate_mission(*definition.mission, entities, result);
    }
    validate_mission_events(definition, entities, result);
    if (!result.empty()) {
        return std::unexpected{std::move(result)};
    }
    return {};
}
} // namespace levels
