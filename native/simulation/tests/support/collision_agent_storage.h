#pragma once

#include <ioj/sim/agent_accessor.h>
#include <ioj/sim/combat_events.h>
#include <ioj/sim/entity_ledger.h>

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
                out.healths[row] = health;
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
                out.healths[row] = health;
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
                out.healths[row] = health;
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
                player_health = health;
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
                out.healths[row] = health;
                break;
            }
            case EntityType::Fighter: {
                auto out{fighters.get_view().columns()};
                out.locations.set(row, location);
                out.aim_directions.set(row, forward_direction(rotation));
                out.healths[row] = health;
                break;
            }
            case EntityType::Turret: {
                auto out{turrets.get_view().columns()};
                out.locations.set(row, location);
                out.rotations.set(row, rotation);
                out.healths[row] = health;
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
                player_health = health;
                break;
            case EntityType::COUNT:
                assert(false);
                break;
        }
    }

    void set_health(EntityUniqueId id, Health health) {
        auto const state{agents.read(id)};
        assert(state);
        set(id, state->location, {}, health);
    }

    void remove(EntityUniqueId id) {
        clock.phase = SimulationPhase::Preparation;
        auto const row{find_row(id)};
        assert(row >= 0);
        switch (id.entity_type()) {
            case EntityType::CapitalShip:
                capitals.remove_at_swap(row, 1);
                break;
            case EntityType::Fighter:
                fighters.remove_at_swap(row, 1);
                break;
            case EntityType::Turret:
                turrets.remove_at_swap(row, 1);
                break;
            case EntityType::TubeSpinner:
                spinners.remove_at_swap(row, 1);
                break;
            case EntityType::PlayerShip:
                player_ids.clear();
                player_health = 0;
                break;
            case EntityType::COUNT:
                assert(false);
                break;
        }
    }

    void publish() {
        clock.phase = SimulationPhase::Preparation;
        indexes.reset();
        indexes.bind(EntityType::CapitalShip, capitals.get_const_view().entity_ids());
        indexes.bind(EntityType::Fighter, fighters.get_const_view().entity_ids());
        indexes.bind(EntityType::Turret, turrets.get_const_view().entity_ids());
        indexes.bind(EntityType::TubeSpinner, spinners.get_const_view().entity_ids());
        indexes.bind(EntityType::PlayerShip, player_ids);
        agents.bind(capitals.get_const_view().columns(),
                    fighters.get_const_view().columns(),
                    turrets.get_const_view().columns(),
                    spinners.get_const_view().columns(),
                    player_ids.empty() ? PlayerAgentView{}
                                       : PlayerAgentView{&player_transform,
                                                         &player_velocity,
                                                         &player_health,
                                                         &player_team,
                                                         player_ids[0]});
        clock.phase = SimulationPhase::Thinking;
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
    AgentAccessor agents{indexes};
    SingleAllocationCapitalEntityData capitals;
    SingleAllocationFighterEntityData fighters;
    SingleAllocationTurretEntityData turrets;
    SingleAllocationSpinnerEntityData spinners;
    std::vector<EntityUniqueId> player_ids;
    Transform3d player_transform;
    ml::Vector3d player_velocity{};
    Health player_health{};
    Team player_team{};
};
}
