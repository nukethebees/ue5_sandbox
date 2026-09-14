#include "test_entity_registry.h"
#include "../support/simulation_test_support.h"

#include <ioj/sim/capital_ships/sim.h>
#include <ioj/sim/registry_entity_data.h>

namespace ioj::sim {
namespace {
constexpr std::array<std::int32_t, 6> expected_team_counts{0, 1, 2, 3, 4, 5};
}

void run_worldless_entity_registry_scenario(ioj::sim::tests::SimulationFixture const& config,
                                            EntityRegistryScenario const scenario) {
    auto data{ioj::sim::tests::make_simulation_data(config)};
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_ships.fighter_spawn_slots_relative_transforms.clear();
    if (scenario != EntityRegistryScenario::TeamCounts) {
        ioj::sim::tests::add_player_spawn(data, ioj::sim::tests::make_player_spawn(config));
    }
    std::int32_t actor_index{};
    for (std::int32_t team_index{};
         team_index < static_cast<std::int32_t>(expected_team_counts.size());
         ++team_index) {
        for (std::int32_t i{}; i < expected_team_counts[team_index]; ++i) {
            ioj::sim::tests::add_capital_spawn(data,
                                               HMM_V3(static_cast<float>(actor_index * 5000),
                                                      static_cast<float>(team_index * 5000),
                                                      4360.f),
                                               static_cast<ioj::sim::Team>(team_index),
                                               -1,
                                               5.f,
                                               60.f);
            ++actor_index;
        }
    }

    ioj::sim::tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    if (scenario == EntityRegistryScenario::TeamCounts) {
        EntityRegistry::TeamCounts counts{};
        EntityRegistry::EntityCounts type_counts{};
        harness.on_end_tick = [&](LevelSim&) {
            counts = harness.get_registry().count_alive_per_team();
            type_counts = harness.get_registry().count_alive_per_team_and_type();
        };
        harness.timeline.finish_at(0.1);
        ioj::sim::tests::expect_true(harness.run_until_timeline_finished(1.0),
                                     "Team-count timeline completes");
        std::int32_t total_count{};
        for (auto const count : counts) {
            total_count += count;
        }
        ioj::sim::tests::expect_equal(15, total_count, "Check entity total");
        for (std::int32_t team_index{};
             team_index < static_cast<std::int32_t>(expected_team_counts.size());
             ++team_index) {
            auto const team{static_cast<ioj::sim::Team>(team_index)};
            ioj::sim::tests::expect_equal(expected_team_counts[team_index],
                                          counts[team_index],
                                          "Count team " + ::testing::PrintToString(team));
            std::int32_t type_count{};
            constexpr auto type_count_limit{std::to_underlying(ioj::sim::EntityType::COUNT)};
            for (std::int32_t type{}; type < type_count_limit; ++type) {
                type_count += type_counts[team_index][type];
            }
            ioj::sim::tests::expect_equal(counts[team_index],
                                          type_count,
                                          "Count team/type matrix for " +
                                              ::testing::PrintToString(team));
        }
        return;
    }

    auto const expected_kills{scenario == EntityRegistryScenario::OnePlayerKill ? 1 : 2};
    auto const* player{harness.get_simulation().get_player_ship_simulation()};
    assert(player);
    auto const player_id{player->unique_entity_id};
    auto const player_handle{player->registry_handle};
    auto const initial_alive_count{harness.get_registry().count_alive()};
    auto const available_targets{harness.get_registry().get_handles_not_in_team(player->team)};
    ioj::sim::tests::expect_greater(static_cast<std::int32_t>(available_targets.size()),
                                    expected_kills - 1,
                                    "Enough non-player-team targets are available");
    auto const targets{std::span<RegistryEntityHandle const>{available_targets}.first(
        static_cast<std::size_t>(expected_kills))};
    struct Sample {
        std::int32_t player_kills{};
        std::int32_t total_kills{};
        std::int32_t alive_count{};
    };
    ml::TimeSeriesData<Sample> samples;
    harness.on_end_tick = [&](LevelSim&) {
        auto const& registry{harness.get_registry()};
        samples.add(harness.get_time(),
                    Sample{static_cast<std::int32_t>(registry.get_kills(player_id)),
                           registry.count_kills(),
                           registry.count_alive()});
    };
    harness.timeline.then_after(0.1, [&] { harness.queue_kills(targets, player_handle); });
    harness.timeline.finish_at(0.35);
    ioj::sim::tests::expect_true(harness.run_until_timeline_finished(1.0),
                                 "Player-kill timeline completes");
    ioj::sim::tests::expect_true(!samples.is_empty(), "Kill samples recorded");
    if (samples.is_empty()) {
        return;
    }
    auto const& before{samples.nearest_value(0.05)};
    auto const& after{samples.nearest_value(0.3)};
    auto const& final{samples.nearest_value(0.35)};
    ioj::sim::tests::expect_equal(0, before.player_kills, "Player kills are zero before event");
    ioj::sim::tests::expect_equal(0, before.total_kills, "Total kills are zero before event");
    ioj::sim::tests::expect_equal(
        initial_alive_count, before.alive_count, "All entities are alive before event");
    ioj::sim::tests::expect_equal(
        expected_kills, after.player_kills, "Kills are attributed to player ship");
    ioj::sim::tests::expect_equal(
        expected_kills, after.total_kills, "Total kill count matches killed entities");
    ioj::sim::tests::expect_equal(initial_alive_count - expected_kills,
                                  after.alive_count,
                                  "Alive count reflects killed entities");
    ioj::sim::tests::expect_equal(
        expected_kills, final.player_kills, "Player kill count remains correct");
    ioj::sim::tests::expect_equal(
        expected_kills, final.total_kills, "Total kill count remains correct");
    ioj::sim::tests::expect_equal(
        initial_alive_count - expected_kills, final.alive_count, "Alive count remains correct");
}

}
