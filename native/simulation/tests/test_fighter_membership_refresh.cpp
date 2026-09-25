#include <ioj/sim/testing/level_sim_test_access.h>
#include "support/simulation_test_support.h"

namespace ioj::sim::tests::fighter_membership_refresh {

auto make_world() -> LevelSimInitData {
    LevelSimInitData data;
    data.grid_geometry = {{16, 16, 4}, {{1000.f, 1000.f, 1000.f}}};
    data.lasers.n_preallocated_instances = 8;
    data.capital_ships.fighter_spawn_slots = 0;
    data.fighters.health = 100;
    data.fighters.speed = 0.f;
    data.fighters.laser.damage = 0;
    data.overlap_response.damage_per_overlap_detection = 1;
    data.participating_teams.add(Team::Green);
    data.participating_teams.add(Team::Red);
    add_capital_spawn(data, {{-1000.f, 0.f, 0.f}}, Team::Green, -1, 60.f, 60.f, 100);
    add_capital_spawn(data, {{-2000.f, 0.f, 0.f}}, Team::Green, -1, 60.f, 60.f, 100);
    add_capital_spawn(data, {{2000.f, 0.f, 0.f}}, Team::Red, -1, 60.f, 60.f, 100);
    for (auto const type : ml::EnumTraits<EntityType>::values) {
        data.entity_bounds.set_half_extents(type, {{10.f, 10.f, 10.f}});
    }
    return data;
}

class FighterMembershipRefresh : public ::testing::Test {
  protected:
    void SetUp() override {
        simulation.finish_initialisation();
        first_parent = simulation.get_capital_ships().get_id(0);
        second_parent = simulation.get_capital_ships().get_id(1);
        enemy = simulation.get_capital_ships().get_id(2);
        spawn(4);
        auto const ids{simulation.get_fighters().get_entity_ids()};
        fighters.assign(ids.begin(), ids.end());
        LevelSimTestAccess::set_fighter_parent(simulation, fighters[2], second_parent);
        LevelSimTestAccess::set_fighter_parent(simulation, fighters[3], second_parent);
        EXPECT_GT(refresh(), 0u);
        expect_membership();
    }

    void spawn(std::int32_t const count) {
        FighterSpawnQueue spawns;
        spawns.add_defaulted(count);
        auto const data{spawns.get_view()};
        for (std::int32_t index{}; index < count; ++index) {
            data.locations.set(index, {{100.f + 100.f * index, 100.f, 0.f}});
            data.teams[index] = Team::Green;
            data.parents[index] = first_parent;
            data.targets[index] = enemy;
        }
        LevelSimTestAccess::commit_fighter_spawns(simulation, spawns.get_const_view());
    }

    auto refresh() -> std::uint64_t {
        auto const before{memory.get_stats().current_root_claim_count};
        ml::FrameScratchScope scope{memory};
        LevelSimTestAccess::refresh_fighter_membership(simulation, scope.scratch());
        return memory.get_stats().current_root_claim_count - before;
    }

    void expect_membership() {
        auto const capitals{simulation.get_capital_ships().get_read_view()};
        auto const fighter_view{simulation.get_fighters().get_read_view()};
        auto const& entities{fighter_view.entities};
        std::vector<EntityUniqueId> flat;
        std::int32_t offset{};
        auto const capital_count{capitals.entities.num()};
        auto const fighter_count{entities.num()};
        for (std::int32_t capital_index{}; capital_index < capital_count; ++capital_index) {
            std::vector<EntityUniqueId> expected;
            for (std::int32_t fighter_index{}; fighter_index < fighter_count; ++fighter_index) {
                if (is_alive(fighter_view.healths.health(fighter_index)) &&
                    entities.parent_ids[fighter_index] ==
                        capitals.entities.entity_ids[capital_index]) {
                    expected.push_back(entities.entity_ids[fighter_index]);
                }
            }
            auto const actual{simulation.get_capital_ships().get_fighter_ids(capital_index)};
            EXPECT_TRUE(std::ranges::equal(actual, expected));
            EXPECT_TRUE(std::ranges::equal(capitals.get_fighter_ids(capital_index), expected));
            EXPECT_EQ(capitals.entities.fighter_id_spans[capital_index],
                      (IndexSpan{offset, static_cast<std::int32_t>(expected.size())}));
            offset += static_cast<std::int32_t>(expected.size());
            flat.insert(flat.end(), expected.begin(), expected.end());
        }
        EXPECT_TRUE(std::ranges::equal(simulation.get_capital_ships().get_fighter_ids(), flat));
    }

    void damage(EntityUniqueId const victim, Health const amount) {
        DirectDamageEvents events;
        events.add_uninitialised(1);
        events.damaged_entities[0] = victim;
        events.damage_amounts[0] = amount;
        LevelSimTestAccess::queue_direct_damage_events(simulation, events.get_const_view());
    }

    void resolve() {
        ml::FrameScratchScope scope{memory};
        LevelSimTestAccess::resolve_ship_damage(simulation, scope.scratch());
    }

    void order_task(EntityUniqueId const fighter, FighterTask const task) {
        FighterOrderQueue orders;
        orders.add(fighter, FighterOrder{1, 0}, task, {});
        LevelSimTestAccess::queue_fighter_orders(simulation, orders);
    }

    LevelSim simulation{make_world()};
    EntityUniqueId first_parent;
    EntityUniqueId second_parent;
    EntityUniqueId enemy;
    std::vector<EntityUniqueId> fighters;
    alignas(ml::FrameMemoryResource::backing_alignment) std::array<std::byte, 64 * 1024> backing{};
    ml::FrameMemoryResource memory{backing};
};

TEST_F(FighterMembershipRefresh, RepeatedRefreshesAndUnchangedTickAllocateNothing) {
    EXPECT_EQ(refresh(), 0u);
    EXPECT_EQ(refresh(), 0u);
    auto const revision{simulation.get_fighters().get_membership_revision()};
    simulation.start();
    simulation.advance(simulation.get_clock().get_tick_period());
    EXPECT_EQ(simulation.get_fighters().get_membership_revision(), revision);
    expect_membership();
    EXPECT_EQ(refresh(), 0u);
}

TEST_F(FighterMembershipRefresh, SpawnCommitsInvalidateAtPreparationBoundary) {
    auto const revision{simulation.get_fighters().get_membership_revision()};
    spawn(2);
    EXPECT_GT(simulation.get_fighters().get_membership_revision(), revision);
    EXPECT_GT(refresh(), 0u);
    expect_membership();
    EXPECT_EQ(simulation.get_capital_ships().get_fighter_ids(0).size(), 4u);
    EXPECT_EQ(refresh(), 0u);
}

TEST_F(FighterMembershipRefresh, DeathInvalidatesBeforePhysicalRemoval) {
    auto const revision{simulation.get_fighters().get_membership_revision()};
    damage(fighters[0], 100);
    resolve();
    ASSERT_GE(simulation.get_agent_indexes().find(fighters[0]), 0);
    EXPECT_GT(simulation.get_fighters().get_membership_revision(), revision);
    EXPECT_GT(refresh(), 0u);
    expect_membership();
    EXPECT_EQ(simulation.get_capital_ships().get_fighter_ids(0).size(), 1u);
    EXPECT_EQ(refresh(), 0u);
    auto const dead_revision{simulation.get_fighters().get_membership_revision()};
    damage(fighters[0], 100);
    resolve();
    EXPECT_EQ(simulation.get_fighters().get_membership_revision(), dead_revision);
    EXPECT_EQ(refresh(), 0u);
}

TEST_F(FighterMembershipRefresh, SwapRemovalPreservesSpansAndStorageOrdering) {
    damage(fighters[0], 100);
    damage(fighters[2], 100);
    resolve();
    EXPECT_GT(refresh(), 0u);
    auto const membership_revision{simulation.get_fighters().get_membership_revision()};
    auto const layout_revision{simulation.get_fighters().get_layout_revision()};
    LevelSimTestAccess::remove_dead_ships(simulation);
    EXPECT_EQ(simulation.get_fighters().get_membership_revision(), membership_revision);
    EXPECT_GT(simulation.get_fighters().get_layout_revision(), layout_revision);
    EXPECT_GT(refresh(), 0u);
    expect_membership();
    EXPECT_EQ(simulation.get_fighters().get_num_instances(), 2);
    EXPECT_EQ(refresh(), 0u);
}

TEST_F(FighterMembershipRefresh, ParentReassignmentMovesOnlyLiveMembership) {
    LevelSimTestAccess::set_fighter_parent(simulation, fighters[0], second_parent);
    EXPECT_GT(refresh(), 0u);
    expect_membership();
    EXPECT_EQ(simulation.get_capital_ships().get_fighter_ids(0).size(), 1u);
    EXPECT_EQ(simulation.get_capital_ships().get_fighter_ids(1).size(), 3u);
    auto const revision{simulation.get_fighters().get_membership_revision()};
    LevelSimTestAccess::set_fighter_parent(simulation, fighters[0], second_parent);
    EXPECT_EQ(simulation.get_fighters().get_membership_revision(), revision);
    EXPECT_EQ(refresh(), 0u);
    damage(fighters[0], 100);
    resolve();
    EXPECT_GT(refresh(), 0u);
    auto const dead_revision{simulation.get_fighters().get_membership_revision()};
    LevelSimTestAccess::set_fighter_parent(simulation, fighters[0], first_parent);
    EXPECT_EQ(simulation.get_fighters().get_membership_revision(), dead_revision);
    EXPECT_EQ(refresh(), 0u);
    expect_membership();
}

TEST_F(FighterMembershipRefresh, CapitalDeathReassignsToSurvivingCapitalAndCompactsSpans) {
    damage(first_parent, 100);
    resolve();
    for (auto const parent : simulation.get_fighters().get_parent_ids()) {
        EXPECT_EQ(parent, second_parent);
    }
    LevelSimTestAccess::remove_dead_ships(simulation);
    EXPECT_GT(refresh(), 0u);
    expect_membership();
    auto const index{simulation.get_agent_indexes().find(second_parent)};
    EXPECT_EQ(simulation.get_capital_ships().get_fighter_ids(index).size(), 4u);
    EXPECT_EQ(refresh(), 0u);
}

TEST_F(FighterMembershipRefresh, SimultaneousOwnerDeathsOrphanSurvivors) {
    damage(first_parent, 100);
    damage(second_parent, 100);
    resolve();
    LevelSimTestAccess::remove_dead_ships(simulation);
    EXPECT_GT(refresh(), 0u);
    expect_membership();
    EXPECT_TRUE(simulation.get_capital_ships().get_fighter_ids().empty());
    EXPECT_EQ(simulation.get_fighters().get_num_instances(), 4);
    EXPECT_EQ(refresh(), 0u);
}

TEST_F(FighterMembershipRefresh, MultipleMutationsNeedOneEventualRebuild) {
    spawn(1);
    LevelSimTestAccess::set_fighter_parent(simulation, fighters[1], second_parent);
    LevelSimTestAccess::set_fighter_parent(simulation, fighters[3], first_parent);
    damage(fighters[0], 100);
    resolve();
    LevelSimTestAccess::remove_dead_ships(simulation);
    EXPECT_GT(refresh(), 0u);
    expect_membership();
    EXPECT_EQ(refresh(), 0u);
    EXPECT_EQ(refresh(), 0u);
}

TEST_F(FighterMembershipRefresh, PendingReparentingChangesMembershipOnlyWhenCommitted) {
    FighterSpawnQueue spawns;
    spawns.add_defaulted(1);
    auto const data{spawns.get_view()};
    data.locations.set(0, {{700.f, 100.f, 0.f}});
    data.teams[0] = Team::Green;
    data.parents[0] = first_parent;
    data.targets[0] = enemy;
    auto const revision{simulation.get_fighters().get_membership_revision()};
    LevelSimTestAccess::queue_fighter_spawns(simulation, spawns.get_const_view());
    LevelSimTestAccess::reassign_pending_fighter_spawns(simulation, first_parent, second_parent);
    EXPECT_EQ(simulation.get_fighters().get_membership_revision(), revision);
    EXPECT_EQ(refresh(), 0u);

    simulation.start();
    simulation.advance(simulation.get_clock().get_tick_period());
    EXPECT_GT(simulation.get_fighters().get_membership_revision(), revision);
    EXPECT_EQ(simulation.get_fighters().get_num_instances(), 5);
    EXPECT_EQ(simulation.get_capital_ships().get_fighter_ids(1).size(), 3u);
    expect_membership();
    EXPECT_EQ(refresh(), 0u);
}

TEST_F(FighterMembershipRefresh, EmptySpawnCommitDoesNotInvalidate) {
    auto const revision{simulation.get_fighters().get_membership_revision()};
    spawn(0);
    EXPECT_EQ(simulation.get_fighters().get_membership_revision(), revision);
    EXPECT_EQ(refresh(), 0u);
    expect_membership();
}

TEST_F(FighterMembershipRefresh, TaskRegroupingPreservesExactListOrderWithoutMembershipChange) {
    auto const revision{simulation.get_fighters().get_membership_revision()};
    auto const layout{simulation.get_fighters().get_layout_revision()};
    order_task(fighters[1], FighterTask::Standby);
    LevelSimTestAccess::prepare_fighters(simulation);
    EXPECT_EQ(simulation.get_fighters().get_membership_revision(), revision);
    EXPECT_GT(simulation.get_fighters().get_layout_revision(), layout);
    EXPECT_GT(refresh(), 0u);
    expect_membership();
    auto const owned{simulation.get_capital_ships().get_fighter_ids(0)};
    ASSERT_EQ(owned.size(), 2u);
    EXPECT_EQ(owned[0], fighters[1]);
    EXPECT_EQ(owned[1], fighters[0]);
    EXPECT_EQ(refresh(), 0u);
}

TEST_F(FighterMembershipRefresh, UnrelatedWritesAndLayoutWithoutPermutationDoNotInvalidate) {
    auto const revision{simulation.get_fighters().get_membership_revision()};
    auto const layout{simulation.get_fighters().get_layout_revision()};
    LevelSimTestAccess::set_fighter_target(simulation, fighters[0], second_parent);
    LevelSimTestAccess::set_fighter_kinematics(simulation, fighters[0], {{500.f, 200.f, 0.f}}, {});
    damage(fighters[0], 1);
    resolve();
    for (auto const fighter : fighters) {
        order_task(fighter, FighterTask::Standby);
    }
    LevelSimTestAccess::prepare_fighters(simulation);
    EXPECT_EQ(simulation.get_fighters().get_membership_revision(), revision);
    EXPECT_EQ(simulation.get_fighters().get_layout_revision(), layout);
    EXPECT_EQ(refresh(), 0u);
    expect_membership();
}

TEST_F(FighterMembershipRefresh, CapitalRemovalWithoutOwnedFightersInvalidatesSpanLayout) {
    auto const revision{simulation.get_fighters().get_membership_revision()};
    damage(enemy, 100);
    resolve();
    LevelSimTestAccess::remove_dead_ships(simulation);
    EXPECT_EQ(simulation.get_fighters().get_membership_revision(), revision);
    EXPECT_GT(refresh(), 0u);
    expect_membership();
    EXPECT_EQ(refresh(), 0u);
}

TEST_F(FighterMembershipRefresh, CapitalRegistrationInitializesNewEmptySpan) {
    CapitalSpawnData spawns;
    spawns.add_defaulted(1);
    auto const data{spawns.get_view()};
    data.locations.set(0, {{3000.f, 0.f, 0.f}});
    data.teams[0] = Team::Green;
    data.healths[0] = 100;
    auto const revision{simulation.get_fighters().get_membership_revision()};
    LevelSimTestAccess::register_capitals(simulation, spawns.get_const_view());
    EXPECT_EQ(simulation.get_fighters().get_membership_revision(), revision);
    EXPECT_GT(refresh(), 0u);
    expect_membership();
    EXPECT_EQ(simulation.get_capital_ships().get_fighter_id_span(3), (IndexSpan{4, 0}));
    EXPECT_EQ(refresh(), 0u);
}

TEST_F(FighterMembershipRefresh, ReconstructedSimulationBuildsItsOwnEmptyCache) {
    LevelSim reconstructed{make_world()};
    reconstructed.finish_initialisation();
    ml::FrameScratchScope scope{memory};
    auto const before{memory.get_stats().current_root_claim_count};
    LevelSimTestAccess::refresh_fighter_membership(reconstructed, scope.scratch());
    EXPECT_GT(memory.get_stats().current_root_claim_count, before);
    auto const after{memory.get_stats().current_root_claim_count};
    LevelSimTestAccess::refresh_fighter_membership(reconstructed, scope.scratch());
    EXPECT_EQ(memory.get_stats().current_root_claim_count, after);
    EXPECT_TRUE(reconstructed.get_capital_ships().get_fighter_ids().empty());
}

} // namespace ioj::sim::tests::fighter_membership_refresh
