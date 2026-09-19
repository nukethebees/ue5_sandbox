#pragma once

#include <ioj/sim/agent_accessor.h>
#include <ioj/sim/combat_events.h>
#include <ioj/sim/entity_ledger.h>
#include <ioj/sim/entity_tables.h>

#include <array>

namespace ioj::sim::tests {
struct CollisionAgentStorage {
    CollisionAgentStorage() { clock.phase = SimulationPhase::Preparation; }

    auto spawn(EntityType type,
               Vector3f location = {},
               Rotator3f rotation = {},
               Health health = 100,
               Team team = Team::Blue) -> EntityUniqueId {
        clock.phase = SimulationPhase::Preparation;
        auto const id{ledger.record_spawn(type, team, health > 0)};
        switch (type) {
            case EntityType::CapitalShip: {
                auto const row{capitals.num()};
                capitals.add_defaulted(1);
                auto out{capitals.get_view().columns()};
                out.entity_ids[row] = id;
                out.locations.set(row, location);
                out.rotations.set(row, rotation);
                add_health(id, out.health_indices[row], health);
                out.teams[row] = team;
                break;
            }
            case EntityType::Fighter: {
                auto const row{fighters.num()};
                fighters.add_defaulted(1);
                auto out{fighters.get_view().columns()};
                out.entity_ids[row] = id;
                out.locations.set(row, location);
                out.aim_directions.set(row, forward_direction(rotation));
                add_health(id, out.health_indices[row], health);
                out.teams[row] = team;
                break;
            }
            case EntityType::Turret: {
                auto const row{turrets.num()};
                turrets.add_defaulted(1);
                auto out{turrets.get_view().columns()};
                out.entity_ids[row] = id;
                out.locations.set(row, location);
                out.rotations.set(row, rotation);
                add_health(id, out.health_indices[row], health);
                out.teams[row] = team;
                break;
            }
            case EntityType::TubeSpinner: {
                auto const row{spinners.num()};
                spinners.add_defaulted(1);
                auto out{spinners.get_view().columns()};
                out.entity_ids[row] = id;
                out.locations.set(row, location);
                out.yaws[row] = rotation.yaw;
                break;
            }
            case EntityType::PlayerShip:
                assert(player_ids.empty());
                player_ids.push_back(id);
                player_transform.location = {location.X, location.Y, location.Z};
                player_transform.rotation =
                    to_quaternion(Rotator3d{rotation.pitch, rotation.yaw, rotation.roll});
                add_health(id, player_health_index, health);
                player_team = team;
                break;
            case EntityType::COUNT:
                assert(false);
                break;
        }
        return id;
    }

    void set(EntityUniqueId id, Vector3f location, Rotator3f rotation, Health health) {
        auto const row{find_row(id)};
        assert(row >= 0);
        switch (id.entity_type()) {
            case EntityType::CapitalShip: {
                auto out{capitals.get_view().columns()};
                out.locations.set(row, location);
                out.rotations.set(row, rotation);
                health_table.get_view(out.health_indices, out.entity_ids).health(row) = health;
                break;
            }
            case EntityType::Fighter: {
                auto out{fighters.get_view().columns()};
                out.locations.set(row, location);
                out.aim_directions.set(row, forward_direction(rotation));
                health_table.get_view(out.health_indices, out.entity_ids).health(row) = health;
                break;
            }
            case EntityType::Turret: {
                auto out{turrets.get_view().columns()};
                out.locations.set(row, location);
                out.rotations.set(row, rotation);
                health_table.get_view(out.health_indices, out.entity_ids).health(row) = health;
                break;
            }
            case EntityType::TubeSpinner: {
                auto out{spinners.get_view().columns()};
                out.locations.set(row, location);
                out.yaws[row] = rotation.yaw;
                break;
            }
            case EntityType::PlayerShip:
                player_transform.location = {location.X, location.Y, location.Z};
                player_transform.rotation =
                    to_quaternion(Rotator3d{rotation.pitch, rotation.yaw, rotation.roll});
                health_table
                    .get_view(std::span<HealthIndex const>{&player_health_index, 1}, player_ids)
                    .health(0) = health;
                break;
            case EntityType::COUNT:
                assert(false);
                break;
        }
    }

    void remove(EntityUniqueId id) {
        clock.phase = SimulationPhase::Preparation;
        auto const row{find_row(id)};
        assert(row >= 0);
        bind_entity_indices();
        switch (id.entity_type()) {
            case EntityType::CapitalShip:
                remove_health(capitals.get_const_view().columns().health_indices[row], id);
                capitals.remove_at_swap(row, 1);
                break;
            case EntityType::Fighter:
                remove_health(fighters.get_const_view().columns().health_indices[row], id);
                fighters.remove_at_swap(row, 1);
                break;
            case EntityType::Turret:
                remove_health(turrets.get_const_view().columns().health_indices[row], id);
                turrets.remove_at_swap(row, 1);
                break;
            case EntityType::TubeSpinner:
                spinners.remove_at_swap(row, 1);
                break;
            case EntityType::PlayerShip:
                player_ids.clear();
                remove_health(player_health_index, id);
                player_health_index = {};
                break;
            case EntityType::COUNT:
                assert(false);
                break;
        }
    }

    void publish() {
        clock.phase = SimulationPhase::Preparation;
        bind_entity_indices();
        agents.bind(capitals.get_const_view().columns(),
                    fighters.get_const_view().columns(),
                    turrets.get_const_view().columns(),
                    spinners.get_const_view().columns(),
                    player_ids.empty() ? PlayerAgentView{}
                                       : PlayerAgentView{&player_transform,
                                                         &player_velocity,
                                                         player_health_index,
                                                         &player_team,
                                                         player_ids[0]});
        clock.phase = SimulationPhase::Thinking;
    }

    void bind_entity_indices() {
        indexes.reset();
        indexes.bind(EntityType::CapitalShip, capitals.get_const_view().entity_ids());
        indexes.bind(EntityType::Fighter, fighters.get_const_view().entity_ids());
        indexes.bind(EntityType::Turret, turrets.get_const_view().entity_ids());
        indexes.bind(EntityType::TubeSpinner, spinners.get_const_view().entity_ids());
        indexes.bind(EntityType::PlayerShip, player_ids);

        entity_tables.bind_health_indices(EntityType::CapitalShip,
                                          capitals.get_const_view().entity_ids(),
                                          capitals.get_view().health_indices());
        entity_tables.bind_health_indices(EntityType::Fighter,
                                          fighters.get_const_view().entity_ids(),
                                          fighters.get_view().health_indices());
        entity_tables.bind_health_indices(EntityType::Turret,
                                          turrets.get_const_view().entity_ids(),
                                          turrets.get_view().health_indices());
        entity_tables.bind_health_indices(EntityType::PlayerShip,
                                          player_ids,
                                          player_ids.empty()
                                              ? std::span<HealthIndex>{}
                                              : std::span<HealthIndex>{&player_health_index, 1});
    }

    void add_health(EntityUniqueId const id, HealthIndex& index, Health const health) {
        health_table.add(std::span<EntityUniqueId const>{&id, 1},
                         std::span<Health const>{&health, 1},
                         std::span<HealthIndex>{&index, 1});
        assert(health_table.contains(index, id));
    }

    void remove_health(HealthIndex const index, EntityUniqueId const id) {
        std::array const rows{0};
        std::array const indices{index};
        std::array const ids{id};
        entity_tables.remove_health_rows(rows, indices, ids);
    }

    auto find_row(EntityUniqueId id) const -> std::int32_t {
        auto find = [id](std::span<EntityUniqueId const> ids) {
            auto const found{std::ranges::find(ids, id)};
            return found == ids.end() ? -1 : static_cast<std::int32_t>(found - ids.begin());
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
            case EntityType::COUNT:
                return -1;
        }
        return -1;
    }

    SimClock clock;
    EntityLedger ledger;
    CombatEvents combat_events{ledger};
    AgentIndexes indexes{clock};
    EntityTables entity_tables{indexes};
    HealthTable& health_table{entity_tables.health};
    AgentAccessor agents{indexes, health_table};
    SingleAllocationCapitalEntityData capitals;
    SingleAllocationFighterEntityData fighters;
    SingleAllocationTurretEntityData turrets;
    SingleAllocationSpinnerEntityData spinners;
    std::vector<EntityUniqueId> player_ids;
    Transform3d player_transform;
    ml::Vector3d player_velocity{};
    HealthIndex player_health_index{};
    Team player_team{};
};
}
