#include <ioj/sim/testing/sim_clock_test_access.h>
#pragma once
#include <ioj/sim/column_math.h>
#include <ioj/sim/combat_events.h>
#include <ioj/sim/entity_ledger.h>
#include <ioj/sim/entity_tables.h>
#include <ioj/sim/testing/entity_observations.h>
#include <ioj/sim/testing/spatial_query_manager_test_access.h>

#include <array>

namespace ioj::sim {
struct CollisionGridTestAccess {
    static void begin(collision::CollisionUniformGrid& grid) { grid.begin_entity_grid(); }
    static void finish(collision::CollisionUniformGrid& grid) { grid.finish_entity_grid(); }
    static void
        size_rows(collision::CollisionUniformGrid& grid, EntityType type, std::size_t count) {
        grid.entity_storage_.entity_row_to_aabb_row[type].assign(
            count, EntityInstanceHandle::invalid_value);
    }
    static void append(collision::CollisionUniformGrid& grid,
                       EntityUniqueId id,
                       std::uint32_t row,
                       collision::WorldAABB bounds) {
        grid.entity_storage_.entity_row_to_aabb_row[id.entity_type()][row] =
            grid.entity_storage_.rebuild_entity_data.num();
        grid.append_entity_bounds(id, bounds);
    }
};
}

namespace ioj::sim::tests {
struct CollisionAgentStorage {
    CollisionAgentStorage() { SimClockTestAccess::set_phase(clock, SimulationPhase::Preparation); }

    auto spawn(EntityType type,
               Vector3f location = {},
               Rotator3f rotation = {},
               Health health = 100,
               Team team = Team::Blue) -> EntityUniqueId {
        SimClockTestAccess::set_phase(clock, SimulationPhase::Preparation);
        auto const id{ledger.record_spawn(type, team, health > 0)};
        switch (type) {
            case EntityType::CapitalShip: {
                auto const row{capitals.num()};
                capitals.add_defaulted(1);
                auto out{capitals.get_view()};
                out.entity_ids()[row] = id;
                set_vector(out.view_locations(), row, location);
                set_rotation(out.view_rotations(), row, rotation);
                health_table.initialise_rows<EntityType::CapitalShip>(row, 1, health);
                out.teams()[row] = team;
                break;
            }
            case EntityType::Fighter: {
                auto const row{fighters.num()};
                fighters.add_defaulted(1);
                auto out{fighters.get_view()};
                out.entity_ids()[row] = id;
                set_vector(out.view_locations(), row, location);
                set_vector(out.view_aim_directions(), row, forward_direction(rotation));
                health_table.initialise_rows<EntityType::Fighter>(row, 1, health);
                out.teams()[row] = team;
                break;
            }
            case EntityType::Turret: {
                auto const row{turrets.num()};
                turrets.add_defaulted(1);
                auto out{turrets.get_view()};
                out.entity_ids()[row] = id;
                set_vector(out.view_locations(), row, location);
                set_rotation(out.view_rotations(), row, rotation);
                health_table.initialise_rows<EntityType::Turret>(row, 1, health);
                out.teams()[row] = team;
                break;
            }
            case EntityType::TubeSpinner: {
                auto const row{spinners.num()};
                spinners.add_defaulted(1);
                auto out{spinners.get_view()};
                out.entity_ids()[row] = id;
                set_vector(out.view_locations(), row, location);
                out.yaws()[row] = rotation.yaw;
                break;
            }
            case EntityType::PlayerShip:
                assert(player_ids.empty());
                player_ids.push_back(id);
                player_transform.location = {location.X, location.Y, location.Z};
                player_transform.rotation =
                    to_quaternion(Rotator3d{rotation.pitch, rotation.yaw, rotation.roll});
                health_table.initialise_rows<EntityType::PlayerShip>(0, 1, health);
                player_team = team;
                break;
            default:
                assert(false);
                break;
        }
        return id;
    }

    void set(EntityUniqueId id, Vector3f location, Rotator3f rotation, Health health) {
        auto const row{find_row(id)};
        assert(row != EntityInstanceHandle::invalid_value);
        switch (id.entity_type()) {
            case EntityType::CapitalShip: {
                auto out{capitals.get_view()};
                set_vector(out.view_locations(), row, location);
                set_rotation(out.view_rotations(), row, rotation);
                health_table.get_view<EntityType::CapitalShip>(out.num()).set_health(row, health);
                break;
            }
            case EntityType::Fighter: {
                auto out{fighters.get_view()};
                set_vector(out.view_locations(), row, location);
                set_vector(out.view_aim_directions(), row, forward_direction(rotation));
                health_table.get_view<EntityType::Fighter>(out.num()).set_health(row, health);
                break;
            }
            case EntityType::Turret: {
                auto out{turrets.get_view()};
                set_vector(out.view_locations(), row, location);
                set_rotation(out.view_rotations(), row, rotation);
                health_table.get_view<EntityType::Turret>(out.num()).set_health(row, health);
                break;
            }
            case EntityType::TubeSpinner: {
                auto out{spinners.get_view()};
                set_vector(out.view_locations(), row, location);
                out.yaws()[row] = rotation.yaw;
                break;
            }
            case EntityType::PlayerShip:
                player_transform.location = {location.X, location.Y, location.Z};
                player_transform.rotation =
                    to_quaternion(Rotator3d{rotation.pitch, rotation.yaw, rotation.roll});
                health_table.get_view<EntityType::PlayerShip>(player_ids.size())
                    .set_health(0, health);
                break;
            default:
                assert(false);
                break;
        }
    }

    void remove(EntityUniqueId id) {
        SimClockTestAccess::set_phase(clock, SimulationPhase::Preparation);
        auto const row{find_row(id)};
        assert(row != EntityInstanceHandle::invalid_value);
        entity_tables.lookups.for_type(id.entity_type()).retire({&id, 1});
        switch (id.entity_type()) {
            case EntityType::CapitalShip:
                health_table.remove_rows<EntityType::CapitalShip>(capitals.num(), {&row, 1});
                capitals.remove_at_swap(row, 1);
                break;
            case EntityType::Fighter:
                health_table.remove_rows<EntityType::Fighter>(fighters.num(), {&row, 1});
                fighters.remove_at_swap(row, 1);
                break;
            case EntityType::Turret:
                health_table.remove_rows<EntityType::Turret>(turrets.num(), {&row, 1});
                turrets.remove_at_swap(row, 1);
                break;
            case EntityType::TubeSpinner:
                spinners.remove_at_swap(row, 1);
                break;
            case EntityType::PlayerShip:
                player_ids.clear();
                health_table.remove_rows<EntityType::PlayerShip>(1, {&row, 1});
                break;
            default:
                assert(false);
                break;
        }
    }

    void publish() {
        SimClockTestAccess::set_phase(clock, SimulationPhase::Preparation);
        auto const capital_rows{capitals.get_const_view()};
        auto const fighter_rows{fighters.get_const_view()};
        auto const turret_rows{turrets.get_const_view()};
        entity_tables.publish<EntityType::CapitalShip>(
            capital_rows.entity_ids(), capital_rows.teams(), 100);
        entity_tables.publish<EntityType::Fighter>(
            fighter_rows.entity_ids(), fighter_rows.teams(), 100);
        entity_tables.publish<EntityType::Turret>(
            turret_rows.entity_ids(), turret_rows.teams(), 100);
        entity_tables.publish<EntityType::TubeSpinner>(
            spinners.get_const_view().entity_ids(), {}, 1);
        entity_tables.publish<EntityType::PlayerShip>(
            player_ids,
            player_ids.empty() ? std::span<Team const>{} : std::span<Team const>{&player_team, 1},
            100);
        SimClockTestAccess::set_phase(clock, SimulationPhase::Thinking);
    }

    auto find_row(EntityUniqueId id) const -> std::uint32_t {
        auto find = [id](std::span<EntityUniqueId const> ids) {
            auto const found{std::ranges::find(ids, id)};
            return found == ids.end() ? EntityInstanceHandle::invalid_value
                                      : static_cast<std::uint32_t>(found - ids.begin());
        };
        switch (id.entity_type()) {
            case EntityType::CapitalShip:
                return find(capitals.get_const_view().entity_ids());
            case EntityType::Fighter:
                return find(fighters.get_const_view().entity_ids());
            case EntityType::Turret:
                return find(turrets.get_const_view().entity_ids());
            case EntityType::TubeSpinner:
                return find(spinners.get_const_view().entity_ids());
            case EntityType::PlayerShip:
                return find(player_ids);
            default:
                return EntityInstanceHandle::invalid_value;
        }
        return EntityInstanceHandle::invalid_value;
    }

    auto player_spatial() const -> std::optional<PlayerSpatialData> {
        if (player_ids.empty()) {
            return {};
        }
        return PlayerSpatialData{player_ids[0],
                                 to_float(player_transform.location),
                                 to_float(player_velocity),
                                 to_quaternion(to_float(player_transform.rotator())),
                                 health_table.get_const_view<EntityType::PlayerShip>(1).health(0)};
    }
    void refresh(SpatialQueryManager& queries) const {
        SpatialQueryManagerTestAccess::set_motion(
            queries, EntityType::CapitalShip, capitals.get_const_view().view_locations());
        SpatialQueryManagerTestAccess::set_motion(queries,
                                                  EntityType::Fighter,
                                                  fighters.get_const_view().view_locations(),
                                                  fighters.get_const_view().view_velocities());
        SpatialQueryManagerTestAccess::set_motion(
            queries, EntityType::Turret, turrets.get_const_view().view_locations());
        SpatialQueryManagerTestAccess::set_motion(
            queries, EntityType::TubeSpinner, spinners.get_const_view().view_locations());
        SpatialQueryManagerTestAccess::set_player(queries, player_spatial());
        rebuild(SpatialQueryManagerTestAccess::uniform_grid(queries),
                SpatialQueryManagerTestAccess::entity_aabbs(queries));
    }
    void rebuild(collision::CollisionUniformGrid& grid,
                 collision::EntityAABBs const& bounds) const {
        CollisionGridTestAccess::begin(grid);
        auto const append{
            [&](EntityUniqueId id, std::uint32_t row, Vector3f location, Quaternion4f orientation) {
                CollisionGridTestAccess::append(
                    grid,
                    id,
                    row,
                    collision::make_entity_world_bounds(
                        bounds, id.entity_type(), location, orientation));
            }};
        auto const add_batch{
            [&](EntityType type, auto entities, HealthConstView healths, auto orientation_at) {
                auto const count{entities.num()};
                auto const ids{entities.entity_ids()};
                auto const locations{entities.view_locations()};
                CollisionGridTestAccess::size_rows(grid, type, count);
                for (std::uint32_t row{}; row < count; ++row) {
                    if (healths.is_empty() || is_alive(healths.health(row))) {
                        append(ids[row], row, locations[row], orientation_at(row));
                    }
                }
            }};
        CollisionGridTestAccess::size_rows(grid, EntityType::PlayerShip, player_ids.size());
        if (auto const player{player_spatial()}; player && is_alive(player->health)) {
            append(player->id, 0, player->location, player->orientation);
        }
        auto const capital_rotations{capitals.get_const_view().view_rotations()};
        add_batch(EntityType::CapitalShip,
                  capitals.get_const_view(),
                  health_table.get_const_view<EntityType::CapitalShip>(capitals.num()),
                  [capital_rotations](auto row) {
                      return to_quaternion(rotation_at(capital_rotations, row));
                  });
        auto const turret_rotations{turrets.get_const_view().view_rotations()};
        add_batch(EntityType::Turret,
                  turrets.get_const_view(),
                  health_table.get_const_view<EntityType::Turret>(turrets.num()),
                  [turret_rotations](auto row) {
                      return to_quaternion(rotation_at(turret_rotations, row));
                  });
        auto const directions{fighters.get_const_view().view_aim_directions()};
        add_batch(EntityType::Fighter,
                  fighters.get_const_view(),
                  health_table.get_const_view<EntityType::Fighter>(fighters.num()),
                  [directions](auto row) {
                      return to_quaternion(direction_to_rotation(directions[row]));
                  });
        auto const yaws{spinners.get_const_view().yaws()};
        add_batch(EntityType::TubeSpinner, spinners.get_const_view(), {}, [yaws](auto row) {
            return to_quaternion(Rotator3f{.yaw = yaws[row]});
        });
        CollisionGridTestAccess::finish(grid);
    }

    SimClock clock;
    EntityLedger ledger;
    CombatEvents combat_events{ledger};
    EntityTables entity_tables{clock};
    HealthTable& health_table{entity_tables.health};
    CapitalEntityData capitals;
    FighterEntityData fighters;
    TurretEntityData turrets;
    SpinnerEntityData spinners;
    std::vector<EntityUniqueId> player_ids;
    Transform3d player_transform;
    ml::Vector3d player_velocity{};
    Team player_team{};
};

inline auto observe_entity(CollisionAgentStorage const& owners, EntityUniqueId const id)
    -> std::optional<EntityObservation> {
    if (id.entity_type() == EntityType::PlayerShip) {
        auto const player{owners.player_spatial()};
        if (!player || player->id != id) {
            return {};
        }
        return EntityObservation{
            player->location, player->velocity, owners.player_team, player->health, 0};
    }
    switch (id.entity_type()) {
        case EntityType::CapitalShip: {
            auto const entities{owners.capitals.get_const_view()};
            return observe_entity_columns(
                id,
                entities.entity_ids(),
                entities.view_locations(),
                {},
                owners.health_table.get_const_view<EntityType::CapitalShip>(entities.num()),
                entities.teams());
        }
        case EntityType::Fighter: {
            auto const entities{owners.fighters.get_const_view()};
            return observe_entity_columns(
                id,
                entities.entity_ids(),
                entities.view_locations(),
                entities.view_velocities(),
                owners.health_table.get_const_view<EntityType::Fighter>(entities.num()),
                entities.teams());
        }
        case EntityType::Turret: {
            auto const entities{owners.turrets.get_const_view()};
            return observe_entity_columns(
                id,
                entities.entity_ids(),
                entities.view_locations(),
                {},
                owners.health_table.get_const_view<EntityType::Turret>(entities.num()),
                entities.teams());
        }
        case EntityType::TubeSpinner: {
            auto const entities{owners.spinners.get_const_view()};
            return observe_entity_columns(
                id, entities.entity_ids(), entities.view_locations(), {}, {}, {});
        }
        default:
            return {};
    }
}

inline auto observe_live_entity(CollisionAgentStorage const& source, EntityUniqueId const id)
    -> std::optional<EntityObservation> {
    auto result{observe_entity(source, id)};
    if (result && is_dead(result->health)) {
        result.reset();
    }
    return result;
}

inline auto entity_is_alive(CollisionAgentStorage const& source, EntityUniqueId const id) -> bool {
    return observe_live_entity(source, id).has_value();
}

inline auto observe_entity_row(CollisionAgentStorage const& source, EntityUniqueId const id)
    -> EntityFrameIndex {
    auto const entity{observe_entity(source, id)};
    return entity ? entity->row : EntityInstanceHandle::invalid_value;
}

}
