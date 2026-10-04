#pragma once

#include <ioj/sim/column_math.h>
#include <ioj/sim/entity_tables.h>
#include <ioj/sim/level_sim.h>
#include <ioj/sim/rotator_math.h>

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
inline auto observe_entity_storage(auto const& access, EntityUniqueId const id)
    -> std::optional<EntityObservation> {
    if (!id.is_valid()) {
        return {};
    }
    auto const observe{[id](std::span<EntityUniqueId const> ids,
                            Vectors3fConstView locations,
                            Vectors3fConstView velocities,
                            HealthConstView healths,
                            std::span<Team const> teams) -> std::optional<EntityObservation> {
        auto const found{std::ranges::find(ids, id)};
        if (found == ids.end()) {
            return {};
        }
        auto const row{static_cast<EntityFrameIndex>(found - ids.begin())};
        return EntityObservation{locations[row],
                                 velocities.is_empty() ? Vector3f{} : velocities[row],
                                 teams.empty() ? Team::White : teams[row],
                                 healths.is_empty() ? 1 : healths.health(row),
                                 row};
    }};
    switch (id.entity_type()) {
        case EntityType::CapitalShip: {
            auto const view{access.get_capitals()};
            return observe(view.entities.entity_ids(),
                           view.entities.view_locations(),
                           {},
                           view.healths,
                           view.entities.teams());
        }
        case EntityType::Fighter: {
            auto const view{access.get_fighters()};
            return observe(view.entities.entity_ids(),
                           view.entities.view_locations(),
                           view.entities.view_velocities(),
                           view.healths,
                           view.entities.teams());
        }
        case EntityType::Turret: {
            auto const view{access.get_turrets()};
            return observe(view.entities.entity_ids(),
                           view.entities.view_locations(),
                           {},
                           view.healths,
                           view.entities.teams());
        }
        case EntityType::TubeSpinner: {
            auto const view{access.get_spinners()};
            return observe(view.entities.entity_ids(), view.entities.view_locations(), {}, {}, {});
        }
        default:
            return {};
    }
}

inline auto observe_entity(LevelSim const& simulation, EntityUniqueId const id)
    -> std::optional<EntityObservation> {
    if (id.entity_type() == EntityType::PlayerShip) {
        auto const* player{simulation.get_player_ship_simulation()};
        if (!player || player->unique_entity_id != id || !player->is_alive()) {
            return {};
        }
        auto const& state{player->get_physical_state()};
        return EntityObservation{to_float(state.transform.location),
                                 to_float(state.velocity),
                                 player->team,
                                 player->get_health().health,
                                 0};
    }
    return observe_entity_storage(simulation.get_read_access(), id);
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
