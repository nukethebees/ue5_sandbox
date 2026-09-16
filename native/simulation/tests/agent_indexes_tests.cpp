#include <ioj/sim/agent_accessor.h>
#include <ioj/sim/agent_indexes.h>

#include <array>
#include <gtest/gtest.h>

namespace ioj::sim::tests {
TEST(AgentAccessor, ReadsAuthoritativeStateAndDistinguishesDeadFromRemoved) {
    SingleAllocationFighterEntityData fighters;
    fighters.add_defaulted(1);
    auto data{fighters.get_view().columns()};
    auto const id{EntityUniqueId::make(0, EntityType::Fighter)};
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
    data.healths[0] = 0;
    EXPECT_EQ(indexes.find(id), 0);
    EXPECT_TRUE(agents.read(id));
    EXPECT_FALSE(agents.read_alive(id));

    clock.phase = SimulationPhase::Preparation;
    indexes.retire(id);
    fighters.remove_at_swap(0, 1);
    indexes.bind(EntityType::Fighter, fighters.get_const_view().entity_ids());
    agents.bind({}, fighters.get_const_view().columns(), {}, {});
    clock.phase = SimulationPhase::Thinking;
    EXPECT_FALSE(agents.read(id));
}

TEST(AgentIndexes, ResolvesOwnerRowsAndRejectsUnissuedOrWrongTypeIds) {
    SimClock clock;
    AgentIndexes indexes{clock};
    std::array const fighters{EntityUniqueId::make(2, EntityType::Fighter),
                              EntityUniqueId::make(5, EntityType::Fighter)};
    std::array const capitals{EntityUniqueId::make(0, EntityType::CapitalShip)};
    indexes.bind(EntityType::Fighter, fighters);
    indexes.bind(EntityType::CapitalShip, capitals);
    clock.phase = SimulationPhase::Thinking;

    EXPECT_EQ(indexes.find(fighters[0]), 0);
    EXPECT_EQ(indexes.find(fighters[1]), 1);
    EXPECT_EQ(indexes.find(capitals[0]), 0);
    EXPECT_EQ(indexes.find({}), -1);
    EXPECT_EQ(indexes.find(EntityUniqueId::make(1, EntityType::Fighter)), -1);
    EXPECT_EQ(indexes.find(EntityUniqueId::make(2, EntityType::CapitalShip)), -1);
    EXPECT_EQ(indexes.find(EntityUniqueId::make(100, EntityType::Fighter)), -1);
}

TEST(AgentIndexes, RemovalAndReorderingCannotAliasOldIdentity) {
    SimClock clock;
    AgentIndexes indexes{clock};
    auto const removed{EntityUniqueId::make(0, EntityType::Fighter)};
    auto const survivor{EntityUniqueId::make(1, EntityType::Fighter)};
    std::array const before{removed, survivor};
    indexes.bind(EntityType::Fighter, before);
    clock.phase = SimulationPhase::Thinking;
    EXPECT_EQ(indexes.find(survivor), 1);

    clock.phase = SimulationPhase::Preparation;
    EXPECT_EQ(indexes.find(survivor), 1);
    EXPECT_EQ(indexes.find(removed), 0);
    indexes.retire(removed);
    auto const newborn{EntityUniqueId::make(2, EntityType::Fighter)};
    std::array const after{survivor, newborn};
    indexes.bind(EntityType::Fighter, after);
    clock.phase = SimulationPhase::Thinking;
    EXPECT_EQ(indexes.find(removed), -1);
    EXPECT_EQ(indexes.find(survivor), 0);
    EXPECT_EQ(indexes.find(newborn), 1);
}
}
