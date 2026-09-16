#pragma once

#include <ioj/sim/agent_accessor.h>
#include <ioj/sim/entity_registry.h>

namespace ioj::sim::tests {
// Legacy collision scenarios author registry updates. Materialize those inputs in
// real owner storage explicitly; collision itself never falls back to registry state.
struct CollisionAgentStorage {
    SimClock clock;
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

    void load(EntityRegistry const& registry) {
        indexes.reset();
        capitals.reset();
        fighters.reset();
        turrets.reset();
        spinners.reset();
        player_ids.clear();
        auto const& data{registry.get_entity_data()};
        auto const generations{registry.get_generations()};
        auto const count{registry.get_num_elements()};
        for (std::int32_t i{}; i < count; ++i) {
            auto const id{registry.get_current_id({i, generations[i]})};
            if (!id.is_valid()) {
                continue;
            }
            switch (id.entity_type()) {
                case EntityType::CapitalShip: {
                    auto const row{capitals.num()};
                    capitals.add_defaulted(1);
                    auto out{capitals.get_view().columns()};
                    out.entity_ids[row] = id;
                    out.locations.set(row, data.locations[i]);
                    out.rotations.set(row, data.rotations[i]);
                    out.healths[row] = data.healths[i];
                    out.teams[row] = data.teams[i];
                    break;
                }
                case EntityType::Fighter: {
                    auto const row{fighters.num()};
                    fighters.add_defaulted(1);
                    auto out{fighters.get_view().columns()};
                    out.entity_ids[row] = id;
                    out.locations.set(row, data.locations[i]);
                    out.aim_directions.set(row, forward_direction(data.rotations[i]));
                    out.healths[row] = data.healths[i];
                    out.teams[row] = data.teams[i];
                    break;
                }
                case EntityType::Turret: {
                    auto const row{turrets.num()};
                    turrets.add_defaulted(1);
                    auto out{turrets.get_view().columns()};
                    out.entity_ids[row] = id;
                    out.locations.set(row, data.locations[i]);
                    out.rotations.set(row, data.rotations[i]);
                    out.healths[row] = data.healths[i];
                    out.teams[row] = data.teams[i];
                    break;
                }
                case EntityType::TubeSpinner: {
                    auto const row{spinners.num()};
                    spinners.add_defaulted(1);
                    auto out{spinners.get_view().columns()};
                    out.entity_ids[row] = id;
                    out.locations.set(row, data.locations[i]);
                    out.yaws[row] = data.rotations[i].yaw;
                    break;
                }
                case EntityType::PlayerShip: {
                    assert(player_ids.empty());
                    player_ids.push_back(id);
                    auto const location{data.locations[i]};
                    auto const rotation{data.rotations[i]};
                    player_transform.location = {location.X, location.Y, location.Z};
                    player_transform.rotation =
                        to_quaternion(Rotator3d{rotation.pitch, rotation.yaw, rotation.roll});
                    player_health = data.healths[i];
                    player_team = data.teams[i];
                    break;
                }
                case EntityType::COUNT:
                    assert(false);
                    break;
            }
        }
        indexes.bind(EntityType::CapitalShip, capitals.get_const_view().entity_ids());
        indexes.bind(EntityType::Fighter, fighters.get_const_view().entity_ids());
        indexes.bind(EntityType::Turret, turrets.get_const_view().entity_ids());
        indexes.bind(EntityType::TubeSpinner, spinners.get_const_view().entity_ids());
        indexes.bind(EntityType::PlayerShip, player_ids);
        agents.bind(capitals.get_const_view().columns(),
                    fighters.get_const_view().columns(),
                    turrets.get_const_view().columns(),
                    spinners.get_const_view().columns(),
                    {&player_transform, &player_velocity, &player_health, &player_team});
    }
};
}
