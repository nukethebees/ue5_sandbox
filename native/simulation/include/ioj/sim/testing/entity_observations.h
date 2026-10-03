#pragma once

#include <ioj/sim/entity_queries.h>
#include <ioj/sim/level_sim.h>

#include <optional>

namespace ioj::sim {
struct EntityObservation {
    Vector3f location{};
    Vector3f velocity{};
    Team team{};
    Health health{};
    EntityFrameIndex row{};
};

// Inspect current owner storage without extending a published frame handle's lifetime.
inline auto observe_entity(EntityTables const& tables, EntityUniqueId const id)
    -> std::optional<EntityObservation> {
    if (!id.is_valid()) {
        return {};
    }
    if (id.entity_type() == EntityType::PlayerShip) {
        auto const source{tables.sources.player};
        if (source.transform == nullptr || source.id != id) {
            return {};
        }
        auto const health{tables.health.get_const_view<EntityType::PlayerShip>(1).health(0)};
        if (is_dead(health)) {
            return {};
        }
        return EntityObservation{to_float(source.transform->location),
                                 to_float(*source.velocity),
                                 *source.team,
                                 health,
                                 0};
    }

    auto const batches{display_batches(tables)};
    for (auto const& batch : batches) {
        if (batch.type != id.entity_type()) {
            continue;
        }
        auto const found{std::ranges::find(batch.ids, id)};
        if (found == batch.ids.end()) {
            return {};
        }
        auto const row{static_cast<EntityFrameIndex>(found - batch.ids.begin())};
        return EntityObservation{vector_at(batch.locations, row),
                                 batch.velocity(row),
                                 batch.team(row),
                                 batch.health(row),
                                 row};
    }
    return {};
}

inline auto observe_entity(LevelSim const& simulation, EntityUniqueId const id)
    -> std::optional<EntityObservation> {
    return observe_entity(simulation.get_entity_tables(), id);
}

inline auto observe_live_entity(auto const& source, EntityUniqueId const id)
    -> std::optional<EntityObservation> {
    auto result{observe_entity(source, id)};
    if (result && is_dead(result->health)) {
        result.reset();
    }
    return result;
}

inline auto entity_is_alive(auto const& source, EntityUniqueId const id) -> bool {
    return observe_live_entity(source, id).has_value();
}

inline auto observe_entity_row(auto const& source, EntityUniqueId const id) -> EntityFrameIndex {
    auto const entity{observe_entity(source, id)};
    return entity ? entity->row : EntityInstanceHandle::invalid_value;
}
} // namespace ioj::sim
