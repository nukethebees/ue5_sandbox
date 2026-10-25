#include "level_processing.h"

#include "mission_processing.h"

#include <array>
#include <format>
#include <unordered_set>

namespace ioj::levels::authoring {
namespace {
void assign_generated_entity_ids(std::vector<EntitySpawnDefinition>& entities) {
    std::unordered_set<EntityId> used_ids;
    used_ids.reserve(entities.size());
    for (auto const& entity : entities) {
        if (!entity.id.empty()) {
            used_ids.insert(entity.id);
        }
    }

    std::array<std::array<std::size_t, EnumTraits<EntityArchetype>::values.size()>,
               EnumTraits<Team>::values.size()>
        next_indices{};
    for (auto& entity : entities) {
        if (!entity.id.empty()) {
            continue;
        }

        auto& index{next_indices[static_cast<std::size_t>(entity.team)]
                                [static_cast<std::size_t>(entity.archetype)]};
        // Reserve explicit IDs before generating names, including IDs on later entities.
        do {
            entity.id =
                EntityId{std::format("{:s}-{:s}-{}", entity.team, entity.archetype, index++)};
        } while (!used_ids.insert(entity.id).second);
    }
}

}

auto detail::process_level(LevelInput input) -> LevelDefinitionReadResult {
    LevelDefinitionComponents result{
        .metadata = {std::move(input.id),
                     std::move(input.title),
                     std::move(input.description),
                     input.par_time},
        .player_entity_id = std::move(input.player),
        .camera = std::move(input.camera),
        .settings = std::move(input.settings),
    };
    Diagnostics errors;
    if (input.unlock) {
        if (input.unlock->empty()) {
            errors.emplace_back(DiagnosticCode::MissingValue,
                                "level.unlock",
                                "Unlock requires at least one criterion");
        }
        for (auto& criterion : *input.unlock) {
            result.unlock_level_ids.push_back(std::move(criterion.level));
        }
    }
    if (input.mission) {
        auto mission{process_mission(std::move(*input.mission), "level.mission")};
        if (mission) {
            result.mission = std::move(*mission);
        } else {
            errors.insert(errors.end(),
                          std::make_move_iterator(mission.error().begin()),
                          std::make_move_iterator(mission.error().end()));
        }
    }
    result.entities.reserve(input.entities.size());
    for (auto& entity : input.entities) {
        result.entities.push_back({
            .id = std::move(entity.id),
            .archetype = entity.archetype,
            .team = entity.team,
            .position = entity.position,
            .rotation = {entity.rotation.x(), entity.rotation.y(), entity.rotation.z()},
            .spawn_time_seconds = entity.spawn_at,
            .overrides = {entity.health,
                          entity.laser_damage,
                          entity.initial_spawn_delay,
                          entity.spawn_cooldown,
                          std::move(entity.target)},
        });
    }
    if (!errors.empty()) {
        return std::unexpected{std::move(errors)};
    }
    assign_generated_entity_ids(result.entities);
    return LevelDefinition::make(std::move(result));
}

}
