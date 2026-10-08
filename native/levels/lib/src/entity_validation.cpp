#include "validation.h"

#include <cmath>
#include <format>

namespace ioj::levels::detail {
namespace {
auto owner(EntitySpawnDefinition const& entity, std::size_t const index) -> std::string {
    return entity.id.empty() ? "Entity at row " + std::to_string(index)
                             : "Entity '" + entity.id.value + "'";
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
}

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
}
