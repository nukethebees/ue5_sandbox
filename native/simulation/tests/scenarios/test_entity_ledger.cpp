#include "../support/simulation_test_support.h"
#include <ioj/sim/capital_ships/sim.h>
#include <ioj/testing/structural_diff.h>

namespace ioj::sim {
using ioj::testing::structurally_equal;
enum class EntityLedgerScenario : std::uint8_t { TeamCounts, OnePlayerKill, TwoPlayerKills };

namespace {
constexpr std::array<std::int32_t, 6> expected_team_counts{0, 1, 2, 3, 4, 5};
}

void run_worldless_entity_ledger_scenario(tests::SimulationFixture const& config,
                                          EntityLedgerScenario const scenario) {
    auto data{tests::make_simulation_data(config)};
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_ships.fighter_spawn_slots_relative_transforms.clear();
    if (scenario != EntityLedgerScenario::TeamCounts) {
        tests::add_player_spawn(data, tests::make_player_spawn(config));
    }
    std::int32_t actor_index{};
    auto const team_count{static_cast<std::int32_t>(expected_team_counts.size())};
    for (std::int32_t team_index{}; team_index < team_count; ++team_index) {
        auto const team_entity_count{expected_team_counts[team_index]};
        for (std::int32_t i{}; i < team_entity_count; ++i) {
            tests::add_capital_spawn(data,
                                     Vector3f{static_cast<float>(actor_index * 5000),
                                              static_cast<float>(team_index * 5000),
                                              4360.f},
                                     static_cast<Team>(team_index),
                                     invalid_level_entity_index,
                                     5.f,
                                     60.f);
            ++actor_index;
        }
    }

    tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    if (scenario == EntityLedgerScenario::TeamCounts) {
        telemetry::TeamCounts counts{};
        telemetry::EntityCounts type_counts{};
        harness.on_end_tick = [&](LevelSim&) {
            counts = harness.get_ledger().count_alive_per_team();
            type_counts = harness.get_ledger().count_alive_per_team_and_type();
        };
        harness.timeline.finish_at(0.1);
        EXPECT_TRUE(harness.run_until_timeline_finished(1.0)) << "Team-count timeline completes";
        telemetry::EntityCounts expected_type_counts{};
        for (std::int32_t team_index{}; team_index < team_count; ++team_index) {
            expected_type_counts[team_index][std::to_underlying(EntityType::CapitalShip)] =
                expected_team_counts[team_index];
        }
        EXPECT_PRED_FORMAT2(structurally_equal, expected_team_counts, counts);
        EXPECT_PRED_FORMAT2(structurally_equal, expected_type_counts, type_counts);
        return;
    }

    auto const expected_kills{scenario == EntityLedgerScenario::OnePlayerKill ? 1 : 2};
    auto const* player{harness.get_simulation().get_player_ship_simulation()};
    assert(player);
    auto const player_id{player->unique_entity_id};
    auto const initial_alive_count{harness.get_ledger().count_alive()};
    std::vector<EntityId> available_targets;
    auto const capitals{harness.get_simulation().get_capital_ships().get_entities()};
    auto const capital_count{capitals.num()};
    auto const teams{capitals.teams()};
    auto const entity_ids{capitals.entity_ids()};
    for (std::uint32_t index{}; index < capital_count; ++index) {
        if (teams[index] != player->team) {
            available_targets.push_back(entity_ids[index]);
        }
    }
    EXPECT_GT(static_cast<std::int32_t>(available_targets.size()), expected_kills - 1)
        << "Enough non-player-team targets are available";
    auto const targets{std::span<EntityId const>{available_targets}.first(
        static_cast<std::size_t>(expected_kills))};
    struct Sample {
        std::int32_t player_kills{};
        std::int32_t total_kills{};
        std::int32_t alive_count{};
    };
    TimeSeriesData<Sample> samples;
    harness.on_end_tick = [&](LevelSim&) {
        auto const& ledger{harness.get_ledger()};
        samples.add(harness.get_time(),
                    Sample{static_cast<std::int32_t>(ledger.get_kills(player_id)),
                           ledger.count_kills(),
                           ledger.count_alive()});
    };
    harness.timeline.then_after(0.1, [&] { harness.queue_kills(targets, player_id); });
    harness.timeline.finish_at(0.35);
    ASSERT_TRUE(harness.run_until_timeline_finished(1.0)) << "Player-kill timeline completes";
    ASSERT_FALSE(samples.is_empty()) << "Kill samples recorded";
    auto const expect_sample{[&](double const time, std::int32_t const kills) {
        SCOPED_TRACE(time);
        auto const& sample{samples.nearest_value(time)};
        EXPECT_EQ(sample.player_kills, kills);
        EXPECT_EQ(sample.total_kills, kills);
        EXPECT_EQ(sample.alive_count, initial_alive_count - kills);
    }};
    expect_sample(0.05, 0);
    expect_sample(0.3, expected_kills);
    expect_sample(0.35, expected_kills);
}

TEST(WorldlessSpaceGameSimulation, EntityLedger_CountsTeams) {
    run_worldless_entity_ledger_scenario(tests::make_fixture(), EntityLedgerScenario::TeamCounts);
}

TEST(WorldlessSpaceGameSimulation, EntityLedger_OnePlayerKill) {
    run_worldless_entity_ledger_scenario(tests::make_fixture(),
                                         EntityLedgerScenario::OnePlayerKill);
}

TEST(WorldlessSpaceGameSimulation, EntityLedger_TwoPlayerKills) {
    run_worldless_entity_ledger_scenario(tests::make_fixture(),
                                         EntityLedgerScenario::TwoPlayerKills);
}

}
