#include <ioj/sim/levels/level_definition.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace ioj::sim::levels {
namespace {
using IdSet = std::unordered_set<std::string>;

void add_error(LevelValidationResult& result,
               LevelValidationErrorCode const code,
               std::string message) {
    result.errors.push_back({code, std::move(message)});
}

auto owner(EntitySpawnDefinition const& entity, std::size_t const index) -> std::string {
    return entity.id.empty() ? "Entity at row " + std::to_string(index)
                             : "Entity '" + entity.id + "'";
}

auto supported_team(std::string const& id) -> bool {
    return id == "white" || id == "red" || id == "green" || id == "blue" || id == "orange" ||
           id == "yellow";
}

enum class Archetype { Player, Capital, Turret, Unsupported };

auto resolve_archetype(std::string const& id) -> Archetype {
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
    std::unordered_map<std::string, std::string> teams_by_id{};
    std::unordered_map<std::string, double> spawn_times_by_id{};
    bool player_found{};
};

auto validate_entities(LevelDefinition const& definition,
                       IdSet const& declared_teams,
                       LevelValidationResult& result) -> EntityValidationState {
    EntityValidationState state;
    state.ids.reserve(definition.entities.size());
    state.teams_by_id.reserve(definition.entities.size());
    state.spawn_times_by_id.reserve(definition.entities.size());

    for (std::size_t index{}; index < definition.entities.size(); ++index) {
        auto const& entity{definition.entities[index]};
        auto const entity_owner{owner(entity, index)};
        auto const is_player{!definition.player_entity_id.empty() &&
                             entity.id == definition.player_entity_id};

        if (!entity.id.empty()) {
            if (!state.ids.insert(entity.id).second) {
                add_error(result,
                          LevelValidationErrorCode::DuplicateEntityId,
                          entity_owner + " duplicates authored entity id '" + entity.id + "'");
            } else {
                state.teams_by_id.emplace(entity.id, entity.team);
                state.spawn_times_by_id.emplace(entity.id, entity.spawn_time_seconds);
            }
        }

        state.player_found = state.player_found || is_player;
        if (!declared_teams.contains(entity.team)) {
            add_error(result,
                      LevelValidationErrorCode::UnknownTeamReference,
                      entity_owner + " references undeclared team '" + entity.team + "'");
        }

        if (entity.archetype.empty()) {
            add_error(result,
                      LevelValidationErrorCode::EmptyArchetypeId,
                      entity_owner + " has an empty archetype id");
        } else {
            auto const archetype{resolve_archetype(entity.archetype)};
            if (archetype == Archetype::Unsupported) {
                add_error(result,
                          LevelValidationErrorCode::UnsupportedArchetype,
                          entity_owner + " uses unsupported archetype '" + entity.archetype + "'");
            } else if (is_player != (archetype == Archetype::Player)) {
                add_error(result,
                          LevelValidationErrorCode::ArchetypeRoleMismatch,
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
                      LevelValidationErrorCode::InvalidPlacement,
                      entity_owner + " has a non-finite position or rotation");
        }
        if (!std::isfinite(entity.spawn_time_seconds) || entity.spawn_time_seconds < 0.0) {
            add_error(result,
                      LevelValidationErrorCode::InvalidSpawnTime,
                      entity_owner + " has an invalid spawn time");
        }
        if (is_player && entity.spawn_time_seconds != 0.0) {
            add_error(result,
                      LevelValidationErrorCode::DelayedPlayerSpawn,
                      "The player entity must spawn at time zero");
        }
    }

    if (!definition.player_entity_id.empty() && !state.player_found) {
        add_error(result,
                  LevelValidationErrorCode::PlayerEntityNotFound,
                  "Player entity '" + definition.player_entity_id + "' is not declared");
    }
    return state;
}

auto validate_references(std::vector<std::string> const& references,
                         std::string_view const role,
                         IdSet const& entity_ids,
                         LevelValidationResult& result) -> IdSet {
    IdSet validated;
    validated.reserve(references.size());
    for (auto const& id : references) {
        if (id.empty() || !entity_ids.contains(id)) {
            add_error(result,
                      LevelValidationErrorCode::MissionEntityNotFound,
                      "Mission " + std::string{role} + " entity '" + id + "' is not declared");
        } else if (!validated.insert(id).second) {
            add_error(result,
                      LevelValidationErrorCode::DuplicateMissionEntityReference,
                      "Mission " + std::string{role} + " entity '" + id + "' is duplicated");
        }
    }
    return validated;
}

void validate_initial_spawns(IdSet const& ids,
                             EntityValidationState const& entities,
                             LevelValidationResult& result) {
    for (auto const& id : ids) {
        auto const found{entities.spawn_times_by_id.find(id)};
        if (found != entities.spawn_times_by_id.end() && found->second > 0.0) {
            add_error(result,
                      LevelValidationErrorCode::MissionEventBeforeEntitySpawn,
                      "Initial mission objective entity '" + id + "' does not spawn at time zero");
        }
    }
}

void validate_mission(LevelMissionDefinition const& mission,
                      EntityValidationState const& entities,
                      LevelValidationResult& result) {
    auto requires_time_limit{false};
    auto uses_kill_count{false};
    switch (mission.mode) {
        case LevelMissionMode::Unspecified:
            add_error(result,
                      LevelValidationErrorCode::MissingMissionMode,
                      "Mission definition has no mode");
            break;
        case LevelMissionMode::SurviveTime:
            requires_time_limit = true;
            if (mission.must_survive_entity_ids.empty()) {
                add_error(result,
                          LevelValidationErrorCode::MissingMissionSurvivors,
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
                      LevelValidationErrorCode::UnsupportedMissionMode,
                      "Mission definition uses an unsupported mode");
            break;
    }

    if (requires_time_limit) {
        if (!mission.time_limit_seconds || !std::isfinite(*mission.time_limit_seconds) ||
            *mission.time_limit_seconds <= 0.0f) {
            add_error(result,
                      LevelValidationErrorCode::InvalidMissionTimeLimit,
                      "Mission time limit must be finite and greater than zero");
        }
    } else if (mission.time_limit_seconds) {
        add_error(result,
                  LevelValidationErrorCode::UnexpectedMissionTimeLimit,
                  "Untimed mission cannot define a time limit");
    }

    if (uses_kill_count) {
        if (mission.hero_entity_ids.empty()) {
            add_error(result,
                      LevelValidationErrorCode::MissingMissionHeroes,
                      "Kill mission has no hero entities");
        }
        if (mission.kill_count && *mission.kill_count <= 0) {
            add_error(result,
                      LevelValidationErrorCode::InvalidMissionKillCount,
                      "Mission kill count must be greater than zero when specified");
        }
    } else if (mission.kill_count) {
        add_error(result,
                  LevelValidationErrorCode::UnexpectedMissionKillCount,
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
                      LevelValidationErrorCode::ConflictingMissionEntityRoles,
                      "Mission entity '" + id +
                          "' cannot be both required to kill and a hero or must-survive entity");
        }
    }

    if (uses_kill_count && !mission.kill_count && !heroes.empty()) {
        std::optional<std::string> hero_team;
        for (auto const& id : heroes) {
            auto const found{entities.teams_by_id.find(id)};
            if (found == entities.teams_by_id.end()) {
                continue;
            }
            if (!hero_team) {
                hero_team = found->second;
            } else if (*hero_team != found->second) {
                add_error(result,
                          LevelValidationErrorCode::AmbiguousAutomaticKillTeams,
                          "Automatic kill count requires all hero entities to share a team");
                break;
            }
        }
    }
}

void validate_mission_events(LevelDefinition const& definition,
                             EntityValidationState const& entities,
                             LevelValidationResult& result) {
    if (definition.mission_events.empty()) {
        return;
    }
    if (!definition.mission) {
        add_error(result,
                  LevelValidationErrorCode::UnexpectedMissionEvent,
                  "Mission objective events require a mission definition");
        return;
    }

    auto const& mission{*definition.mission};
    auto must_survive{
        validate_references(mission.must_survive_entity_ids, "must-survive", entities.ids, result)};
    auto required{validate_references(
        mission.required_kill_entity_ids, "required-kill", entities.ids, result)};
    for (auto const& event : definition.mission_events) {
        if (!std::isfinite(event.time_seconds) || event.time_seconds < 0.0) {
            add_error(result,
                      LevelValidationErrorCode::InvalidMissionEventTime,
                      "Mission objective event time must be finite and non-negative");
        }
        if (event.kill_target_increase < 0 ||
            (event.kill_target_increase > 0 && !mission.kill_count)) {
            add_error(result,
                      LevelValidationErrorCode::InvalidMissionKillIncrease,
                      "Mission kill target increases require an explicit kill count and must be "
                      "non-negative");
        }
        if (mission.time_limit_seconds && event.time_seconds > *mission.time_limit_seconds) {
            add_error(result,
                      LevelValidationErrorCode::InvalidMissionEventTime,
                      "Mission objective event occurs after the mission time limit");
        }

        auto validate_event = [&](std::vector<std::string> const& references,
                                  IdSet& same_role,
                                  IdSet const& conflicting_role,
                                  std::string_view const role) {
            for (auto const& id : references) {
                auto const spawn{entities.spawn_times_by_id.find(id)};
                if (spawn == entities.spawn_times_by_id.end()) {
                    add_error(result,
                              LevelValidationErrorCode::MissionEntityNotFound,
                              "Mission event " + std::string{role} + " entity '" + id +
                                  "' is not declared");
                    continue;
                }
                if (spawn->second > event.time_seconds) {
                    add_error(result,
                              LevelValidationErrorCode::MissionEventBeforeEntitySpawn,
                              "Mission event references entity '" + id + "' before it spawns");
                }
                if (same_role.contains(id)) {
                    add_error(result,
                              LevelValidationErrorCode::DuplicateMissionEntityReference,
                              "Mission event " + std::string{role} + " entity '" + id +
                                  "' is duplicated");
                } else if (conflicting_role.contains(id)) {
                    add_error(result,
                              LevelValidationErrorCode::ConflictingMissionEntityRoles,
                              "Mission entity '" + id + "' has conflicting roles");
                } else {
                    same_role.insert(id);
                }
            }
        };
        validate_event(event.must_survive_entity_ids, must_survive, required, "must-survive");
        validate_event(event.required_kill_entity_ids, required, must_survive, "required-kill");
    }
}
} // namespace

auto validate_level(LevelDefinition const& definition) -> LevelValidationResult {
    LevelValidationResult result;
    if (definition.metadata.id.empty()) {
        add_error(
            result, LevelValidationErrorCode::MissingLevelId, "Level definition has no stable id");
    }
    auto title{definition.metadata.title};
    title.erase(title.begin(), std::find_if(title.begin(), title.end(), [](unsigned char const c) {
                    return std::isspace(c) == 0;
                }));
    title.erase(std::find_if(title.rbegin(),
                             title.rend(),
                             [](unsigned char const c) { return std::isspace(c) == 0; })
                    .base(),
                title.end());
    if (title.empty()) {
        add_error(result, LevelValidationErrorCode::MissingTitle, "Level definition has no title");
    }
    if (definition.metadata.par_time_seconds) {
        auto const par_time{*definition.metadata.par_time_seconds};
        if (!std::isfinite(par_time) || par_time <= 0.0f) {
            add_error(result,
                      LevelValidationErrorCode::InvalidParTime,
                      "Level par time must be finite and greater than zero");
        }
        if (definition.player_entity_id.empty() || !definition.mission) {
            add_error(result,
                      LevelValidationErrorCode::UnexpectedParTime,
                      "Level par time requires a player-controlled mission");
        }
    }

    IdSet unlock_ids;
    for (auto const& id : definition.unlock_level_ids) {
        if (id.empty()) {
            add_error(result,
                      LevelValidationErrorCode::MissingUnlockLevelId,
                      "Level-completed unlock criterion has no level id");
            continue;
        }
        if (id == definition.metadata.id) {
            add_error(result,
                      LevelValidationErrorCode::SelfUnlockDependency,
                      "Level '" + id + "' requires itself to be completed");
        }
        if (!unlock_ids.insert(id).second) {
            add_error(result,
                      LevelValidationErrorCode::DuplicateUnlockCriterion,
                      "Level-completed criterion for '" + id + "' is duplicated");
        }
    }

    IdSet teams;
    teams.reserve(definition.teams.size());
    for (std::size_t index{}; index < definition.teams.size(); ++index) {
        auto const& team{definition.teams[index]};
        if (team.empty()) {
            add_error(result,
                      LevelValidationErrorCode::EmptyTeamId,
                      "Team " + std::to_string(index) + " has an empty id");
            continue;
        }
        if (!teams.insert(team).second) {
            add_error(result,
                      LevelValidationErrorCode::DuplicateTeamId,
                      "Team " + std::to_string(index) + " duplicates team id '" + team + "'");
            continue;
        }
        if (!supported_team(team)) {
            add_error(result,
                      LevelValidationErrorCode::UnsupportedTeamId,
                      "Team " + std::to_string(index) + " uses unsupported team id '" + team + "'");
        }
    }

    auto const has_player{!definition.player_entity_id.empty()};
    auto const has_camera{definition.camera.has_value()};
    if (!has_player && !has_camera) {
        add_error(result,
                  LevelValidationErrorCode::MissingViewpoint,
                  "Level definition has neither a player nor an initial camera");
    } else if (has_player && has_camera) {
        add_error(result,
                  LevelValidationErrorCode::ConflictingViewpoints,
                  "Level definition cannot have both a player and an initial camera");
    }

    auto const entities{validate_entities(definition, teams, result)};
    if (definition.camera) {
        auto const& camera{*definition.camera};
        if (camera.target_entity_ids.empty()) {
            add_error(result,
                      LevelValidationErrorCode::MissingCameraTarget,
                      "Initial camera has no target entities");
        }
        IdSet targets;
        for (std::size_t index{}; index < camera.target_entity_ids.size(); ++index) {
            auto const& id{camera.target_entity_ids[index]};
            if (!targets.insert(id).second) {
                add_error(result,
                          LevelValidationErrorCode::DuplicateCameraTarget,
                          "Initial camera target " + std::to_string(index) +
                              " duplicates entity '" + id + "'");
                continue;
            }
            if (id.empty() || !entities.ids.contains(id)) {
                add_error(result,
                          LevelValidationErrorCode::CameraTargetNotFound,
                          "Initial camera target '" + id + "' is not declared");
            }
        }
        if (!std::isfinite(camera.distance) || camera.distance <= 0.0) {
            add_error(result,
                      LevelValidationErrorCode::InvalidCameraDistance,
                      "Initial camera distance must be finite and greater than zero");
        }
        auto const& direction{camera.offset_direction};
        auto const finite{std::isfinite(direction.x) && std::isfinite(direction.y) &&
                          std::isfinite(direction.z)};
        auto const nearly_zero{std::abs(direction.x) <= 1.0e-4 && std::abs(direction.y) <= 1.0e-4 &&
                               std::abs(direction.z) <= 1.0e-4};
        if (!finite || nearly_zero) {
            add_error(result,
                      LevelValidationErrorCode::InvalidCameraOffsetDirection,
                      "Initial camera offset direction must be finite and non-zero");
        }
    }
    if (definition.mission) {
        validate_mission(*definition.mission, entities, result);
    }
    validate_mission_events(definition, entities, result);
    return result;
}
} // namespace levels
