#include "../support/simulation_test_support.h"
#include <ioj/sim/capital_ships/sim.h>
#include <ioj/testing/structural_diff.h>

#include <memory_resource>

namespace ioj::sim {
using ioj::testing::structurally_equal;
TEST(WorldlessSpaceGameSimulation, EntityLedgerCountsTeams) {
    tests::SimulationScenario data{tests::make_simulation_data(tests::make_fixture())};
    TeamCounts const expected_team_counts{std::array{0, 1, 2, 3, 4, 5}};
    EntityCounts expected_type_counts{};
    std::int32_t actor_index{};
    for (auto const team : EnumTraits<Team>::values) {
        auto const team_entity_count{expected_team_counts[team]};
        expected_type_counts[team, EntityType::CapitalShip] = team_entity_count;
        for (std::int32_t i{}; i < team_entity_count; ++i) {
            tests::add_capital_spawn(data,
                                     Vector3f{static_cast<float>(actor_index * 5000),
                                              static_cast<float>(std::to_underlying(team) * 5000),
                                              4360.f},
                                     team,
                                     5.f,
                                     60.f);
            ++actor_index;
        }
    }

    LevelSim simulation{std::move(data).compile(), *std::pmr::new_delete_resource()};
    simulation.start();
    simulation.advance(0.1);
    EXPECT_PRED_FORMAT2(structurally_equal,
                        expected_type_counts,
                        simulation.entity_ledger().count_alive_per_team_and_type());
}

void run_player_kills(KillCount const expected_kills) {
    auto const config{tests::make_fixture()};
    tests::SimulationScenario data{tests::make_simulation_data(config)};
    auto player{tests::make_player_spawn(config)};
    player.team = Team::Blue;
    tests::add_player_spawn(data, player);
    for (KillCount index{}; index < expected_kills; ++index) {
        tests::add_capital_spawn(
            data, {30000.f * static_cast<float>(index + 1), 0.f, 0.f}, Team::Red, 60.f);
    }

    tests::WorldlessSimulationTest harness{std::move(data).compile()};
    auto& simulation{harness.simulation()};
    auto const player_id{simulation.player_ship_simulation()->unique_entity_id};
    auto const& ledger{harness.ledger()};
    auto const expect_counts{[&](KillCount const kills) {
        EXPECT_EQ(ledger.kills(player_id), kills);
        EXPECT_EQ(ledger.count_kills(Team::Blue), kills);
        EXPECT_EQ(ledger.count_alive(), expected_kills + 1 - kills);
    }};
    expect_counts(0);
    harness.queue_kills(simulation.capital_ships().get_entities().entity_ids(), player_id);
    simulation.start();
    auto const dt{simulation.clock().tick_period()};
    simulation.advance(dt);
    expect_counts(expected_kills);
    simulation.advance(dt);
    expect_counts(expected_kills);
}

TEST(WorldlessSpaceGameSimulation, EntityLedgerOnePlayerKill) {
    run_player_kills(1);
}

TEST(WorldlessSpaceGameSimulation, EntityLedgerTwoPlayerKills) {
    run_player_kills(2);
}

}
