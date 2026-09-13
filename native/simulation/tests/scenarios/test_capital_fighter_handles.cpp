#include "test_capital_fighter_handles.h"
#include <set>
#include "../support/simulation_test_support.h"

#include <sandbox/simulation/entities/TestEntityRegistry.h>
#include <sandbox/simulation/ships/capital/TestCapitalShipsSimulation.h>
#include <sandbox/simulation/ships/fighters/TestCapitalShipFightersSimulation.h>

/*
This test relies on a long spawn delay to ensure more fighters are not spawned.
The assumption is that there is one wave of fighters total.
*/

namespace ml {
namespace capital_fighter_handles_test {
inline constexpr std::int32_t collision_resilient_health{1'000'000};
}

/* **************************************** */
// Simultaneous capital deaths
/* **************************************** */
void run_worldless_simultaneous_capital_reassignment(
    ml::simulation_tests::SimulationFixture const& config) {
    struct FSample {
        std::vector<FRegistryEntityHandle> capitals{};
        std::vector<std::vector<FRegistryEntityHandle>> owned_fighters{};
        std::vector<ml::simulation::Team> capital_teams{};
        std::vector<ml::simulation::Team> fighter_teams{};
        std::vector<std::int32_t> span_starts{};
        std::vector<FRegistryEntityHandle> all_owned_fighters{};
        std::vector<FRegistryEntityHandle> fighters{};
    };

    auto data{ml::simulation_tests::make_simulation_data(config)};
    data.capital_ships.spawn_delay = 6000.f;
    data.capital_ships.max_health = capital_fighter_handles_test::collision_resilient_health;
    data.fighters.laser.damage = 0;
    data.fighters.health = capital_fighter_handles_test::collision_resilient_health;
    for (std::int32_t i{}; i < 4; ++i) {
        ml::simulation_tests::add_capital_spawn(
            data,
            ml::simulation::Vector3f{{static_cast<float>((i % 2 == 0 ? -1.0 : 1.0) * 250000.0),
                                      static_cast<float>((i / 2) * 200000.0),
                                      0.f}},
            i % 2 == 0 ? ml::simulation::Team::Green : ml::simulation::Team::Red,
            i ^ 1,
            0.f,
            6000.f,
            capital_fighter_handles_test::collision_resilient_health);
    }

    ml::simulation_tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    TimeSeriesData<FSample> samples;
    std::vector<FRegistryEntityHandle> killed_capitals{};
    harness.on_end_tick = [&](FLevelSimulation& simulation) {
        auto const& capitals{simulation.get_capital_ships()};
        auto const& registry{simulation.get_entity_registry()};
        FSample sample;
        auto const count{capitals.get_num_instances()};
        for (std::int32_t i{}; i < count; ++i) {
            auto const handle{capitals.get_handle(i)};
            sample.capitals.push_back(handle);
            sample.capital_teams.push_back(registry.get_team(handle));
            sample.span_starts.push_back(capitals.get_capital_fighter_handle_span(i).start());
            auto const owned{capitals.get_fighter_handles(i)};
            sample.owned_fighters.emplace_back(owned.begin(), owned.end());
            for (auto const fighter : capitals.get_fighter_handles(i)) {
                sample.fighter_teams.push_back(registry.get_team(fighter));
            }
        }
        auto const owned{capitals.get_fighter_handles()};
        sample.all_owned_fighters.insert(
            sample.all_owned_fighters.end(), owned.begin(), owned.end());
        auto const live{simulation.get_capital_ship_fighters().get_handles()};
        sample.fighters.insert(sample.fighters.end(), live.begin(), live.end());
        samples.add(harness.get_time(), std::move(sample));
    };
    harness.timeline.at(1.0, [&] {
        auto const& capitals{harness.get_simulation().get_capital_ships()};
        auto const& registry{harness.get_registry()};
        for (auto const team : {ml::simulation::Team::Green, ml::simulation::Team::Red}) {
            FRegistryEntityHandle victim;
            auto const count{capitals.get_num_instances()};
            for (std::int32_t i{}; i < count; ++i) {
                auto const handle{capitals.get_handle(i)};
                if (registry.get_team(handle) == team) {
                    victim = handle;
                    if (team == ml::simulation::Team::Red) {
                        break;
                    }
                }
            }
            if (ml::simulation_tests::expect_true(victim.is_valid(),
                                                  "Team has a capital to kill")) {
                killed_capitals.push_back(victim);
            }
        }
        harness.queue_kills(std::span<FRegistryEntityHandle const>{
            killed_capitals.data(),
            static_cast<std::size_t>(static_cast<std::int32_t>(killed_capitals.size()))});
    });
    harness.timeline.finish_at(2.0);
    ml::simulation_tests::expect_true(harness.run_until_timeline_finished(10.0),
                                      "Capital reassignment timeline completes");
    ml::simulation_tests::expect_true(samples.num() > 1,
                                      "Capital reassignment samples are recorded");
    if (::testing::Test::HasFailure()) {
        return;
    }

    auto const& before{samples.value_at(samples.nearest_index(0.5))};
    auto const& after{samples.values().back()};
    ml::simulation_tests::expect_equal(
        4, static_cast<std::int32_t>(before.capitals.size()), "Two capitals per team spawn");
    ml::simulation_tests::expect_equal(
        2, static_cast<std::int32_t>(after.capitals.size()), "Both killed capitals are removed");
    ml::simulation_tests::expect_greater(
        static_cast<std::int32_t>(before.fighters.size()), 0, "One fighter wave spawns");
    ml::simulation_tests::expect_equal(static_cast<std::int32_t>(before.fighters.size()),
                                       static_cast<std::int32_t>(after.fighters.size()),
                                       "All fighters survive");
    std::set<FRegistryEntityHandle> seen;
    std::int32_t offset{};
    auto const count{static_cast<std::int32_t>(after.capitals.size())};
    for (std::int32_t i{}; i < count; ++i) {
        ml::simulation_tests::expect_true(
            !std::ranges::contains(killed_capitals, after.capitals[i]), "Owner capital survives");
        ml::simulation_tests::expect_equal(
            offset, after.span_starts[i], "Ownership spans are contiguous");
        std::int32_t expected_count{};
        auto const before_count{static_cast<std::int32_t>(before.capitals.size())};
        for (std::int32_t j{}; j < before_count; ++j) {
            if (before.capital_teams[j] == after.capital_teams[i]) {
                expected_count += static_cast<std::int32_t>(before.owned_fighters[j].size());
            }
        }
        ml::simulation_tests::expect_equal(
            expected_count,
            static_cast<std::int32_t>(after.owned_fighters[i].size()),
            "Survivor owns both original waves on its team");
        for (auto const fighter : after.owned_fighters[i]) {
            ml::simulation_tests::expect_true(!std::ranges::contains(seen, fighter),
                                              "Fighter ownership is unique");
            seen.insert(fighter);
            ml::simulation_tests::expect_true(std::ranges::contains(before.fighters, fighter),
                                              "Original fighter is preserved");
            ml::simulation_tests::expect_true(std::ranges::contains(after.fighters, fighter),
                                              "Owned fighter is in simulation");
            ml::simulation_tests::expect_equal(after.capital_teams[i],
                                               after.fighter_teams[offset],
                                               "Fighter belongs to owner's team");
            if (ml::simulation_tests::expect_true(
                    (offset >= 0 &&
                     static_cast<std::size_t>(offset) < after.all_owned_fighters.size()),
                    "Span is within flat ownership array")) {
                ml::simulation_tests::expect_equal(
                    fighter, after.all_owned_fighters[offset], "Span matches flat ownership array");
            }
            ++offset;
        }
    }
    ml::simulation_tests::expect_equal(static_cast<std::int32_t>(after.fighters.size()),
                                       static_cast<std::int32_t>(seen.size()),
                                       "Every fighter has one owner");
    ml::simulation_tests::expect_equal(offset,
                                       static_cast<std::int32_t>(after.all_owned_fighters.size()),
                                       "Spans cover ownership array");
}

/* **************************************** */
// Capital fighter handle lifecycle
/* **************************************** */
void run_worldless_capital_fighter_handles(ml::simulation_tests::SimulationFixture const& config,
                                           ECapitalFighterHandlesScenario const scenario) {
    auto data{ml::simulation_tests::make_simulation_data(config)};
    data.capital_ships.spawn_delay = 10.f;
    data.capital_ships.max_health = 10000;
    data.fighters.speed = 2000.f;
    data.fighters.laser.max_distance = 15000.f;
    auto const green_index{ml::simulation_tests::add_capital_spawn(
        data, {{-260800.f, -5060.f, 4360.f}}, ml::simulation::Team::Green, 1, 0.f, 6000.f)};
    ml::simulation_tests::add_capital_spawn(
        data, {{245260.f, -451630.f, 4360.f}}, ml::simulation::Team::Red, green_index, 0.f, 6000.f);
    ml::simulation_tests::add_capital_spawn(
        data, {{300450.f, 214000.f, 4360.f}}, ml::simulation::Team::Red, green_index, 0.f, 6000.f);

    ml::simulation_tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    auto const& capitals{harness.get_simulation().get_capital_ships()};
    auto const& fighters{harness.get_simulation().get_capital_ship_fighters()};
    std::vector<FRegistryEntityHandle> destroyed{};
    std::vector<FRegistryEntityHandle> kept{};
    std::vector<FRegistryEntityHandle> green_fighters_before_capital_kill{};
    auto initial_checked{false};
    auto fighter_kill_checked{scenario == ECapitalFighterHandlesScenario::KillCapital};
    auto capital_kill_checked{scenario == ECapitalFighterHandlesScenario::KillFightersOnly};

    harness.timeline.at(0.2, [&] {
        ml::simulation_tests::expect_equal(
            3, capitals.get_num_instances(), "Three capitals are registered");
        auto const expected_fighters{3 * capitals.get_fighter_spawn_slots()};
        ml::simulation_tests::expect_equal(expected_fighters,
                                           fighters.get_num_instances(),
                                           "Every capital spawns its fighter slots");
        ml::simulation_tests::expect_equal(
            expected_fighters,
            static_cast<std::int32_t>(capitals.get_fighter_handles().size()),
            "Capital-owned and simulation fighter counts match");
        for (std::int32_t i{}; i < capitals.get_num_instances(); ++i) {
            ml::simulation_tests::expect_not_equal(capitals.get_handle(i),
                                                   capitals.get_target_handle(i),
                                                   "Capital does not target itself",
                                                   i);
        }
        for (auto const target : fighters.get_target_handles()) {
            ml::simulation_tests::expect_true(target.is_valid(), "Spawned fighter has a target");
        }
        initial_checked = true;
    });

    auto next_time{0.4};
    if (scenario != ECapitalFighterHandlesScenario::KillCapital) {
        harness.timeline.at(next_time, [&] {
            auto const handles{capitals.get_fighter_handles()};
            for (std::int32_t i{}; i < static_cast<std::int32_t>(handles.size()); ++i) {
                (i % 2 == 0 ? destroyed : kept).push_back(handles[i]);
            }
            harness.queue_kills(std::span<FRegistryEntityHandle const>{
                destroyed.data(),
                static_cast<std::size_t>(static_cast<std::int32_t>(destroyed.size()))});
        });
        next_time += 0.2;
        harness.timeline.at(next_time, [&] {
            ml::simulation_tests::expect_equal(static_cast<std::int32_t>(kept.size()),
                                               fighters.get_num_instances(),
                                               "Killed fighters are removed from the simulation");
            ml::simulation_tests::expect_equal(
                static_cast<std::int32_t>(kept.size()),
                static_cast<std::int32_t>(capitals.get_fighter_handles().size()),
                "Killed fighters are removed from capital ownership");
            for (auto const handle : destroyed) {
                ml::simulation_tests::expect_true(harness.get_registry().is_valid_dead(handle),
                                                  "Destroyed fighter is dead");
            }
            fighter_kill_checked = true;
        });
    }

    if (scenario != ECapitalFighterHandlesScenario::KillFightersOnly) {
        next_time += 0.2;
        harness.timeline.at(next_time, [&] {
            auto const main_index{capitals.find_first_index_on_team(ml::simulation::Team::Green)};
            assert(main_index.has_value());
            auto const owned{capitals.get_fighter_handles(*main_index)};
            green_fighters_before_capital_kill =
                std::vector<FRegistryEntityHandle>{owned.begin(), owned.end()};
            harness.queue_kills(std::array{capitals.get_target_handle(*main_index)});
        });
        next_time += 0.5;
        harness.timeline.at(next_time, [&] {
            ml::simulation_tests::expect_equal(
                2, capitals.get_num_instances(), "Killed capital is removed from the simulation");
            auto const main_index{capitals.find_first_index_on_team(ml::simulation::Team::Green)};
            ml::simulation_tests::expect_true(main_index.has_value(), "Green capital survives");
            if (main_index.has_value()) {
                auto const remaining{capitals.get_fighter_handles(*main_index)};
                ml::simulation_tests::expect_equal(
                    static_cast<std::int32_t>(green_fighters_before_capital_kill.size()),
                    static_cast<std::int32_t>(remaining.size()),
                    "Surviving capital keeps its fighters");
                for (auto const handle : remaining) {
                    ml::simulation_tests::expect_true(
                        std::ranges::contains(green_fighters_before_capital_kill, handle),
                        "Surviving fighter retains capital ownership");
                    ml::simulation_tests::expect_true(fighters.get_target_handle(handle).is_valid(),
                                                      "Surviving fighter retargets");
                }
            }
            capital_kill_checked = true;
        });
    }
    harness.timeline.finish_at(next_time + 0.1);
    ml::simulation_tests::expect_true(harness.run_until_timeline_finished(next_time + 0.2),
                                      "Capital fighter handle timeline completes");
    ml::simulation_tests::expect_true(initial_checked, "Initial fighter ownership is checked");
    ml::simulation_tests::expect_true(fighter_kill_checked, "Fighter removal is checked");
    ml::simulation_tests::expect_true(capital_kill_checked, "Capital removal is checked");
}
}
