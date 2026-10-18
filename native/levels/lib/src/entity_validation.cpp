#include "validation.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace ioj::levels::detail {
namespace {
auto owner(EntitySpawnDefinition const& entity, std::size_t const index) -> std::string {
    return entity.id.empty() ? "Entity at row " + std::to_string(index)
                             : "Entity '" + entity.id.value + "'";
}
}

auto validate_entities(LevelDefinitionComponents const& definition, Diagnostics& result)
    -> EntityValidationState {
    EntityValidationState state;
    state.by_id.reserve(definition.entities.size());

    auto const entity_count{definition.entities.size()};
    for (std::size_t index{}; index < entity_count; ++index) {
        auto const& entity{definition.entities[index]};
        auto const entity_owner{owner(entity, index)};
        auto const error_begin{result.size()};
        auto const is_player{!definition.player_entity_id.empty() &&
                             entity.id == definition.player_entity_id};

        if (!entity.id.empty()) {
            if (!state.by_id.emplace(entity.id, EntityFacts{entity.team, entity.spawn_time_seconds})
                     .second) {
                result.emplace_back(DiagnosticCode::DuplicateEntityId,
                                    "level.entities.id",
                                    entity_owner + " duplicates authored entity id '" +
                                        entity.id.value + "'");
            }
        }

        state.player_found = state.player_found || is_player;
        if (!std::ranges::contains(EnumTraits<Team>::values, entity.team)) {
            result.emplace_back(DiagnosticCode::UnsupportedTeam,
                                "level.entities.team",
                                entity_owner + " has an invalid team");
        }

        if (!std::ranges::contains(EnumTraits<EntityArchetype>::values, entity.archetype)) {
            result.emplace_back(DiagnosticCode::UnsupportedArchetype,
                                "level.entities.archetype",
                                entity_owner + " has an invalid archetype");
        } else if (is_player != (entity.archetype == EntityArchetype::PlayerFighter)) {
            result.emplace_back(DiagnosticCode::ArchetypeRoleMismatch,
                                "level.entities.archetype",
                                entity_owner +
                                    " has an archetype incompatible with its player role");
        }

        auto const placement_is_finite{
            is_finite(entity.position) && std::isfinite(entity.rotation.pitch) &&
            std::isfinite(entity.rotation.yaw) && std::isfinite(entity.rotation.roll)};
        if (!placement_is_finite) {
            result.emplace_back(DiagnosticCode::InvalidPlacement,
                                "level.entities.position",
                                entity_owner + " has a non-finite position or rotation");
        }
        if (!std::isfinite(entity.spawn_time_seconds) || entity.spawn_time_seconds < 0.0) {
            result.emplace_back(DiagnosticCode::InvalidSpawnTime,
                                "level.entities.spawn-at",
                                entity_owner + " has an invalid spawn time");
        }
        if (is_player && entity.spawn_time_seconds != 0.0) {
            result.emplace_back(DiagnosticCode::DelayedPlayerSpawn,
                                "level.entities.spawn-at",
                                "The player entity must spawn at time zero");
        }

        auto const& overrides{entity.overrides};
        if (overrides.health && *overrides.health <= 0) {
            result.emplace_back(DiagnosticCode::InvalidNumber,
                                "level.entities.health",
                                "Initial health must be positive");
        }
        if (overrides.laser_damage && *overrides.laser_damage < 0) {
            result.emplace_back(DiagnosticCode::InvalidNumber,
                                "level.entities.laser-damage",
                                "Laser damage must be nonnegative");
        }
        if (overrides.laser_damage && entity.archetype != EntityArchetype::StaticTurret) {
            result.emplace_back(DiagnosticCode::UnknownProperty,
                                "level.entities.laser-damage",
                                "Laser damage overrides require a static turret");
        }
        if (overrides.initial_spawn_delay && (!std::isfinite(*overrides.initial_spawn_delay) ||
                                              *overrides.initial_spawn_delay < 0.0f)) {
            result.emplace_back(DiagnosticCode::InvalidNumber,
                                "level.entities.initial-spawn-delay",
                                "Initial spawn delay must be finite and nonnegative");
        }
        if (overrides.spawn_cooldown &&
            (!std::isfinite(*overrides.spawn_cooldown) || *overrides.spawn_cooldown < 0.0f)) {
            result.emplace_back(DiagnosticCode::InvalidNumber,
                                "level.entities.spawn-cooldown",
                                "Spawn cooldown must be finite and nonnegative");
        }
        if (entity.archetype != EntityArchetype::CapitalShip &&
            (overrides.initial_spawn_delay || overrides.spawn_cooldown ||
             !overrides.target_entity_id.empty())) {
            result.emplace_back(DiagnosticCode::UnknownProperty,
                                "level.entities",
                                "Fighter spawn and target overrides require a capital ship");
        }
        if (!overrides.target_entity_id.empty()) {
            auto const target{std::ranges::find(
                definition.entities, overrides.target_entity_id, &EntitySpawnDefinition::id)};
            if (target == definition.entities.end() ||
                target->archetype != EntityArchetype::CapitalShip || target->team == entity.team ||
                target->spawn_time_seconds > entity.spawn_time_seconds) {
                result.emplace_back(
                    DiagnosticCode::InvalidSymbol,
                    "level.entities.target",
                    "Target must name an enemy capital spawning at or before this ship");
            }
        }

        auto const error_end{result.size()};
        for (auto error_index{error_begin}; error_index < error_end; ++error_index) {
            auto& path{result[error_index].node_path};
            path.insert(std::string_view{"level.entities"}.size(), std::format("[{}]", index));
        }
    }

    if (!definition.player_entity_id.empty() && !state.player_found) {
        result.emplace_back(DiagnosticCode::PlayerEntityNotFound,
                            "level.player",
                            "Player entity '" + definition.player_entity_id.value +
                                "' is not declared");
    }
    return state;
}
}
