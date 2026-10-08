#include "validation.h"
#include <ioj/ascii.h>

#include <cmath>

namespace ioj::levels {
namespace {
auto supported_team(TeamId const& id) -> bool {
    return id.value == "white" || id.value == "red" || id.value == "green" || id.value == "blue" ||
           id.value == "orange" || id.value == "yellow";
}
}

auto validate_level(LevelDefinition const& definition) -> std::expected<void, Diagnostics> {
    Diagnostics result;
    if (definition.metadata.id.empty()) {
        detail::add_error(result,
                          "level.id",
                          DiagnosticCode::MissingLevelId,
                          "Level definition has no stable id");
    }
    if (ioj::blank(definition.metadata.title)) {
        detail::add_error(
            result, "level.title", DiagnosticCode::MissingTitle, "Level definition has no title");
    }
    if (definition.metadata.par_time_seconds) {
        auto const par_time{*definition.metadata.par_time_seconds};
        if (!std::isfinite(par_time) || par_time <= 0.0f) {
            detail::add_error(result,
                              "level.par-time",
                              DiagnosticCode::InvalidParTime,
                              "Level par time must be finite and greater than zero");
        }
        if (definition.player_entity_id.empty() || !definition.mission) {
            detail::add_error(result,
                              "level.par-time",
                              DiagnosticCode::UnexpectedParTime,
                              "Level par time requires a player-controlled mission");
        }
    }
    if (definition.collision_grid) {
        detail::validate_collision_grid(*definition.collision_grid, result);
    }

    std::unordered_set<LevelId> unlock_ids;
    for (auto const& id : definition.unlock_level_ids) {
        if (id.empty()) {
            detail::add_error(result,
                              "level.unlock",
                              DiagnosticCode::MissingUnlockLevelId,
                              "Level-completed unlock criterion has no level id");
            continue;
        }
        if (id == definition.metadata.id) {
            detail::add_error(result,
                              "level.unlock",
                              DiagnosticCode::SelfUnlockDependency,
                              "Level '" + id.value + "' requires itself to be completed");
        }
        if (!unlock_ids.insert(id).second) {
            detail::add_error(result,
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
            detail::add_error(result,
                              "level.teams",
                              DiagnosticCode::EmptyTeamId,
                              "Team " + std::to_string(index) + " has an empty id");
            continue;
        }
        if (!teams.insert(team).second) {
            detail::add_error(result,
                              "level.teams",
                              DiagnosticCode::DuplicateTeamId,
                              "Team " + std::to_string(index) + " duplicates team id '" +
                                  team.value + "'");
            continue;
        }
        if (!supported_team(team)) {
            detail::add_error(result,
                              "level.teams",
                              DiagnosticCode::UnsupportedTeamId,
                              "Team " + std::to_string(index) + " uses unsupported team id '" +
                                  team.value + "'");
        }
    }

    auto const has_player{!definition.player_entity_id.empty()};
    auto const has_camera{definition.camera.has_value()};
    if (!has_player && !has_camera) {
        detail::add_error(result,
                          "level",
                          DiagnosticCode::MissingViewpoint,
                          "Level definition has neither a player nor an initial camera");
    } else if (has_player && has_camera) {
        detail::add_error(result,
                          "level",
                          DiagnosticCode::ConflictingViewpoints,
                          "Level definition cannot have both a player and an initial camera");
    }

    auto const entities{detail::validate_entities(definition, teams, result)};
    if (definition.camera) {
        detail::validate_camera(*definition.camera, entities, result);
    }
    if (definition.mission) {
        detail::validate_mission(*definition.mission, entities, result);
    }
    detail::validate_mission_events(definition, entities, result);
    if (!result.empty()) {
        return std::unexpected{std::move(result)};
    }
    return {};
}
} // namespace ioj::levels
