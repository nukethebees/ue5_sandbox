#include "validation.h"

#include <cassert>
#include <cmath>
#include <format>

namespace ioj::levels::detail {
namespace {
auto validate_references(std::vector<EntityId> const& references,
                         std::string_view const role,
                         EntityValidationState const& entities,
                         Diagnostics& result) -> IdSet {
    IdSet validated;
    validated.reserve(references.size());
    for (auto const& id : references) {
        if (id.empty() || !entities.by_id.contains(id)) {
            result.emplace_back(DiagnosticCode::MissionEntityNotFound,
                                "level.mission",
                                "Mission " + std::string{role} + " entity '" + id.value +
                                    "' is not declared");
        } else if (!validated.insert(id).second) {
            result.emplace_back(DiagnosticCode::DuplicateMissionEntityReference,
                                "level.mission",
                                "Mission " + std::string{role} + " entity '" + id.value +
                                    "' is duplicated");
        }
    }
    return validated;
}

void validate_initial_spawns(IdSet const& ids,
                             EntityValidationState const& entities,
                             Diagnostics& result) {
    for (auto const& id : ids) {
        auto const found{entities.by_id.find(id)};
        assert(found != entities.by_id.end());
        if (found->second.spawn_time_seconds > 0.0) {
            result.emplace_back(DiagnosticCode::MissionEventBeforeEntitySpawn,
                                "level.mission",
                                "Initial mission objective entity '" + id.value +
                                    "' does not spawn at time zero");
        }
    }
}

}

auto validate_mission(LevelMissionDefinition const& mission,
                      EntityValidationState const& entities,
                      Diagnostics& result) -> MissionReferences {
    auto requires_time_limit{false};
    auto uses_kill_count{false};
    switch (mission.mode) {
        case LevelMissionMode::Unspecified:
            result.emplace_back(DiagnosticCode::MissingMissionMode,
                                "level.mission.mode",
                                "Mission definition has no mode");
            break;
        case LevelMissionMode::SurviveTime:
            requires_time_limit = true;
            if (mission.must_survive_entity_ids.empty()) {
                result.emplace_back(DiagnosticCode::MissingMissionSurvivors,
                                    "level.mission.must-survive",
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
            result.emplace_back(DiagnosticCode::UnsupportedMissionMode,
                                "level.mission.mode",
                                "Mission definition uses an unsupported mode");
            break;
    }

    if (requires_time_limit) {
        if (!mission.time_limit_seconds || !std::isfinite(*mission.time_limit_seconds) ||
            *mission.time_limit_seconds <= 0.0f) {
            result.emplace_back(DiagnosticCode::InvalidMissionTimeLimit,
                                "level.mission.time-limit",
                                "Mission time limit must be finite and greater than zero");
        }
    } else if (mission.time_limit_seconds) {
        result.emplace_back(DiagnosticCode::UnexpectedMissionTimeLimit,
                            "level.mission.time-limit",
                            "Untimed mission cannot define a time limit");
    }

    if (uses_kill_count) {
        if (mission.hero_entity_ids.empty()) {
            result.emplace_back(DiagnosticCode::MissingMissionHeroes,
                                "level.mission.heroes",
                                "Kill mission has no hero entities");
        }
        if (mission.kill_count && *mission.kill_count <= 0) {
            result.emplace_back(DiagnosticCode::InvalidMissionKillCount,
                                "level.mission.kill-count",
                                "Mission kill count must be greater than zero when specified");
        }
    } else if (mission.kill_count) {
        result.emplace_back(DiagnosticCode::UnexpectedMissionKillCount,
                            "level.mission.kill-count",
                            "Survive-time mission cannot define a kill count");
    }

    auto const heroes{validate_references(mission.hero_entity_ids, "hero", entities, result)};
    auto survivors{
        validate_references(mission.must_survive_entity_ids, "must-survive", entities, result)};
    auto required{
        validate_references(mission.required_kill_entity_ids, "required-kill", entities, result)};
    validate_initial_spawns(heroes, entities, result);
    validate_initial_spawns(survivors, entities, result);
    validate_initial_spawns(required, entities, result);

    for (auto const& id : required) {
        if (heroes.contains(id) || survivors.contains(id)) {
            result.emplace_back(
                DiagnosticCode::ConflictingMissionEntityRoles,
                "level.mission",
                "Mission entity '" + id.value +
                    "' cannot be both required to kill and a hero or must-survive entity");
        }
    }

    if (uses_kill_count && !mission.kill_count && !heroes.empty()) {
        std::optional<Team> hero_team;
        for (auto const& id : heroes) {
            auto const found{entities.by_id.find(id)};
            assert(found != entities.by_id.end());
            if (!hero_team) {
                hero_team = found->second.team;
            } else if (*hero_team != found->second.team) {
                result.emplace_back(
                    DiagnosticCode::AmbiguousAutomaticKillTeams,
                    "level.mission.heroes",
                    "Automatic kill count requires all hero entities to share a team");
                break;
            }
        }
    }

    return {std::move(survivors), std::move(required)};
}

void validate_mission_events(LevelDefinition const& definition,
                             EntityValidationState const& entities,
                             MissionReferences references,
                             Diagnostics& result) {
    if (definition.mission_events.empty()) {
        return;
    }
    if (!definition.mission) {
        result.emplace_back(DiagnosticCode::UnexpectedMissionEvent,
                            "level.mission-events",
                            "Mission objective events require a mission definition");
        return;
    }

    auto const& mission{*definition.mission};
    auto& must_survive{references.survivors};
    auto& required{references.required};
    auto const event_count{definition.mission_events.size()};
    for (std::size_t event_index{}; event_index < event_count; ++event_index) {
        auto const& event{definition.mission_events[event_index]};
        auto const error_begin{result.size()};
        if (!std::isfinite(event.time_seconds) || event.time_seconds < 0.0) {
            result.emplace_back(DiagnosticCode::InvalidMissionEventTime,
                                "level.mission-events.at",
                                "Mission objective event time must be finite and non-negative");
        }
        if (event.kill_target_increase < 0 ||
            (event.kill_target_increase > 0 && !mission.kill_count)) {
            result.emplace_back(
                DiagnosticCode::InvalidMissionKillIncrease,
                "level.mission-events.increase-kill-count",
                "Mission kill target increases require an explicit kill count and must be "
                "non-negative");
        }
        if (mission.time_limit_seconds && event.time_seconds > *mission.time_limit_seconds) {
            result.emplace_back(DiagnosticCode::InvalidMissionEventTime,
                                "level.mission-events.at",
                                "Mission objective event occurs after the mission time limit");
        }

        auto validate_event = [&](std::vector<EntityId> const& references,
                                  IdSet& same_role,
                                  IdSet const& conflicting_role,
                                  std::string_view const role) {
            for (auto const& id : references) {
                auto const spawn{entities.by_id.find(id)};
                if (spawn == entities.by_id.end()) {
                    result.emplace_back(DiagnosticCode::MissionEntityNotFound,
                                        "level.mission",
                                        "Mission event " + std::string{role} + " entity '" +
                                            id.value + "' is not declared");
                    continue;
                }
                if (spawn->second.spawn_time_seconds > event.time_seconds) {
                    result.emplace_back(DiagnosticCode::MissionEventBeforeEntitySpawn,
                                        "level.mission",
                                        "Mission event references entity '" + id.value +
                                            "' before it spawns");
                }
                if (same_role.contains(id)) {
                    result.emplace_back(DiagnosticCode::DuplicateMissionEntityReference,
                                        "level.mission",
                                        "Mission event " + std::string{role} + " entity '" +
                                            id.value + "' is duplicated");
                } else if (conflicting_role.contains(id)) {
                    result.emplace_back(DiagnosticCode::ConflictingMissionEntityRoles,
                                        "level.mission",
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
}
