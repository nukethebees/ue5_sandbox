#include "validation.h"
#include <ioj/ascii.h>

#include <algorithm>
#include <cmath>

namespace ioj::levels {

auto participating_teams(LevelDefinition const& definition) -> std::vector<Team> {
    std::vector<Team> result;
    for (auto const& entity : definition.entities) {
        if (!std::ranges::contains(result, entity.team)) {
            result.emplace_back(entity.team);
        }
    }
    std::ranges::sort(result);
    return result;
}

auto validate_level(LevelDefinition const& definition) -> std::expected<void, Diagnostics> {
    Diagnostics result;
    if (definition.metadata.id.empty()) {
        result.emplace_back(
            DiagnosticCode::MissingLevelId, "level.id", "Level definition has no stable id");
    }
    if (ioj::blank(definition.metadata.title)) {
        result.emplace_back(
            DiagnosticCode::MissingTitle, "level.title", "Level definition has no title");
    }
    if (definition.metadata.par_time_seconds) {
        auto const par_time{*definition.metadata.par_time_seconds};
        if (!std::isfinite(par_time) || par_time <= 0.0f) {
            result.emplace_back(DiagnosticCode::InvalidParTime,
                                "level.par-time",
                                "Level par time must be finite and greater than zero");
        }
        if (definition.player_entity_id.empty() || !definition.mission) {
            result.emplace_back(DiagnosticCode::UnexpectedParTime,
                                "level.par-time",
                                "Level par time requires a player-controlled mission");
        }
    }
    if (definition.collision_grid) {
        detail::validate_collision_grid(*definition.collision_grid, result);
    }

    std::unordered_set<LevelId> unlock_ids;
    for (auto const& id : definition.unlock_level_ids) {
        if (id.empty()) {
            result.emplace_back(DiagnosticCode::MissingUnlockLevelId,
                                "level.unlock",
                                "Level-completed unlock criterion has no level id");
            continue;
        }
        if (id == definition.metadata.id) {
            result.emplace_back(DiagnosticCode::SelfUnlockDependency,
                                "level.unlock",
                                "Level '" + id.value + "' requires itself to be completed");
        }
        if (!unlock_ids.insert(id).second) {
            result.emplace_back(DiagnosticCode::DuplicateUnlockCriterion,
                                "level.unlock",
                                "Level-completed criterion for '" + id.value + "' is duplicated");
        }
    }

    auto const has_player{!definition.player_entity_id.empty()};
    auto const has_camera{definition.camera.has_value()};
    if (!has_player && !has_camera) {
        result.emplace_back(DiagnosticCode::MissingViewpoint,
                            "level",
                            "Level definition has neither a player nor an initial camera");
    } else if (has_player && has_camera) {
        result.emplace_back(DiagnosticCode::ConflictingViewpoints,
                            "level",
                            "Level definition cannot have both a player and an initial camera");
    }

    auto const entities{detail::validate_entities(definition, result)};
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
