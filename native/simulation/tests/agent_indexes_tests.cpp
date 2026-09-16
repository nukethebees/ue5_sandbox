#include <ioj/sim/agent_accessor.h>
#include <ioj/sim/agent_indexes.h>

#include <array>
#include <gtest/gtest.h>
#include "support/collision_agent_storage.h"

namespace ioj::sim::tests {
TEST(AgentAccessor, ReadsAuthoritativeStateAndDistinguishesDeadFromRemoved) {
    SingleAllocationFighterEntityData fighters;
    fighters.add_defaulted(1);
    auto data{fighters.get_view().columns()};
    auto const id{
        EntityUniqueId::make(entity_identity_offset(EntityType::Fighter, 0), EntityType::Fighter)};
    data.entity_ids[0] = id;
    data.locations.set(0, {{10.f, 20.f, 30.f}});
    data.velocities.set(0, {{1.f, 2.f, 3.f}});
    data.aim_directions.set(0, {{1.f, 0.f, 0.f}});
    data.healths[0] = 100;
    data.teams[0] = Team::Green;
    SimClock clock;
    AgentIndexes indexes{clock};
    AgentAccessor agents{indexes};
    indexes.bind(EntityType::Fighter, data.entity_ids);
    agents.bind({}, fighters.get_const_view().columns(), {}, {});
    clock.phase = SimulationPhase::Thinking;

    ASSERT_TRUE(agents.read_alive(id));
    EXPECT_FLOAT_EQ(agents.read(id)->location.X, 10.f);
    data.locations.xs[0] = 42.f;
    EXPECT_FLOAT_EQ(agents.read(id)->location.X, 42.f);
    EXPECT_FLOAT_EQ(agents.read(id)->velocity.Y, 2.f);
    EXPECT_EQ(agents.read(id)->team, Team::Green);
    auto check_bulk = [&] {
        std::array const ids{
            id,
            EntityUniqueId{},
            id,
            EntityUniqueId::make(entity_identity_offset(EntityType::CapitalShip, 0),
                                 EntityType::CapitalShip),
            EntityUniqueId::make(entity_identity_offset(EntityType::Fighter, 999),
                                 EntityType::Fighter)};
        std::array<std::int32_t, ids.size()> order{};
        std::array<std::uint8_t, ids.size()> alive{};
        Vectors3f locations;
        Vectors3f velocities;
        locations.add_defaulted(static_cast<std::int32_t>(ids.size()));
        velocities.add_defaulted(static_cast<std::int32_t>(ids.size()));
        agents.gather_targets(ids, order, {locations.get_view(), velocities.get_view(), {}, alive});
        for (std::size_t i{}; i < ids.size(); ++i) {
            auto const expected{agents.read_alive(ids[i])};
            EXPECT_EQ(static_cast<bool>(alive[i]), expected.has_value());
            EXPECT_FLOAT_EQ(locations[static_cast<std::int32_t>(i)].X,
                            expected ? expected->location.X : 0.f);
            EXPECT_FLOAT_EQ(velocities[static_cast<std::int32_t>(i)].Y,
                            expected ? expected->velocity.Y : 0.f);
        }
        agents.gather_targets({}, {}, {});
    };
    check_bulk();
    data.healths[0] = 0;
    EXPECT_EQ(indexes.find(id), 0);
    EXPECT_TRUE(agents.read(id));
    EXPECT_FALSE(agents.read_alive(id));
    check_bulk();

    clock.phase = SimulationPhase::Preparation;
    indexes.retire(id);
    fighters.remove_at_swap(0, 1);
    indexes.bind(EntityType::Fighter, fighters.get_const_view().entity_ids());
    agents.bind({}, fighters.get_const_view().columns(), {}, {});
    clock.phase = SimulationPhase::Thinking;
    EXPECT_FALSE(agents.read(id));
    check_bulk();

    auto const type_count{static_cast<std::int32_t>(EntityType::COUNT)};
    CollisionAgentStorage owners;
    std::array<EntityUniqueId, static_cast<std::size_t>(EntityType::COUNT)> spawned{};
    for (std::int32_t i{}; i < type_count; ++i) {
        spawned[static_cast<std::size_t>(i)] = owners.spawn(static_cast<EntityType>(i),
                                                            {{static_cast<float>(i + 1), 2.f, 3.f}},
                                                            {},
                                                            100,
                                                            Team::Green);
    }
    owners.publish();
    std::vector<EntityUniqueId> ids;
    std::vector<std::int32_t> order(static_cast<std::size_t>(type_count * 2));
    std::vector<std::uint8_t> alive(order.size());
    std::vector<Team> teams(order.size());
    Vectors3f locations;
    locations.add_defaulted(type_count * 2);
    for (std::int32_t i{}; i < type_count * 2; ++i) {
        auto const type{static_cast<EntityType>(type_count - 1 - i % type_count)};
        ids.push_back(spawned[static_cast<std::size_t>(type)]);
    }
    auto const original{ids};
    owners.agents.gather_targets(ids, order, {locations.get_view(), {}, teams, alive});
    EXPECT_EQ(ids, original);
    EXPECT_TRUE(std::ranges::is_sorted(order, {}, [&](auto const row) { return ids[row]; }));
    for (std::int32_t row{}; row < type_count * 2; ++row) {
        auto const expected{owners.agents.read_alive(ids[row])};
        ASSERT_TRUE(expected);
        EXPECT_TRUE(alive[row]);
        EXPECT_FLOAT_EQ(locations[row].X, expected->location.X);
        EXPECT_EQ(teams[row], expected->team);
    }
}

TEST(AgentIndexes, ResolvesOwnerRowsAndRejectsUnissuedOrWrongTypeIds) {
    SimClock clock;
    AgentIndexes indexes{clock};
    std::array const fighters{
        EntityUniqueId::make(entity_identity_offset(EntityType::Fighter, 2), EntityType::Fighter),
        EntityUniqueId::make(entity_identity_offset(EntityType::Fighter, 5), EntityType::Fighter)};
    std::array const capitals{EntityUniqueId::make(
        entity_identity_offset(EntityType::CapitalShip, 0), EntityType::CapitalShip)};
    indexes.bind(EntityType::Fighter, fighters);
    indexes.bind(EntityType::CapitalShip, capitals);
    clock.phase = SimulationPhase::Thinking;

    EXPECT_EQ(indexes.find(fighters[0]), 0);
    EXPECT_EQ(indexes.find(fighters[1]), 1);
    EXPECT_EQ(indexes.find(capitals[0]), 0);
    EXPECT_EQ(indexes.find({}), -1);
    EXPECT_EQ(indexes.find(EntityUniqueId::make(entity_identity_offset(EntityType::Fighter, 1),
                                                EntityType::Fighter)),
              -1);
    EXPECT_EQ(indexes.find(EntityUniqueId::make(entity_identity_offset(EntityType::CapitalShip, 2),
                                                EntityType::CapitalShip)),
              -1);
    EXPECT_EQ(indexes.find(EntityUniqueId::make(entity_identity_offset(EntityType::Fighter, 100),
                                                EntityType::Fighter)),
              -1);
}

TEST(AgentIndexes, RemovalAndReorderingCannotAliasOldIdentity) {
    SimClock clock;
    AgentIndexes indexes{clock};
    auto const removed{
        EntityUniqueId::make(entity_identity_offset(EntityType::Fighter, 0), EntityType::Fighter)};
    auto const survivor{
        EntityUniqueId::make(entity_identity_offset(EntityType::Fighter, 1), EntityType::Fighter)};
    std::array const before{removed, survivor};
    indexes.bind(EntityType::Fighter, before);
    clock.phase = SimulationPhase::Thinking;
    EXPECT_EQ(indexes.find(survivor), 1);

    clock.phase = SimulationPhase::Preparation;
    EXPECT_EQ(indexes.find(survivor), 1);
    EXPECT_EQ(indexes.find(removed), 0);
    indexes.retire(removed);
    auto const newborn{
        EntityUniqueId::make(entity_identity_offset(EntityType::Fighter, 2), EntityType::Fighter)};
    std::array const after{survivor, newborn};
    indexes.bind(EntityType::Fighter, after);
    clock.phase = SimulationPhase::Thinking;
    EXPECT_EQ(indexes.find(removed), -1);
    EXPECT_EQ(indexes.find(survivor), 0);
    EXPECT_EQ(indexes.find(newborn), 1);

    auto const rows{indexes.group(EntityType::Fighter)};
    auto const base{entity_identity_offsets[EntityType::Fighter]};
    EXPECT_EQ(rows[newborn.index() - base], 1u);
    EXPECT_EQ(rows[removed.index() - base], AgentIndexes::invalid_index);
    EXPECT_EQ(rows[survivor.index() - base], 0u);
}
}
