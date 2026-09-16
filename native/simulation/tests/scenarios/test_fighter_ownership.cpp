#include "test_fighter_ownership.h"
#include <set>
#include "../support/simulation_test_support.h"

#include <ioj/sim/capital_ships/sim.h>
#include <ioj/sim/fighters/sim.h>

/*
This test relies on a long spawn delay to ensure more fighters are not spawned.
The assumption is that there is one wave of fighters total.
*/

namespace ioj::sim {
namespace fighter_ownership_test {
inline constexpr std::int32_t collision_resilient_health{1'000'000};
}

/* **************************************** */
// Simultaneous capital deaths
/* **************************************** */
void run_worldless_simultaneous_capital_reassignment(tests::SimulationFixture const& config) {
    struct Sample {
        std::vector<EntityUniqueId> capitals{};
        std::vector<std::vector<EntityUniqueId>> owned_fighters{};
        std::vector<Team> capital_teams{};
        std::vector<Team> fighter_teams{};
        std::vector<std::int32_t> span_starts{};
        std::vector<EntityUniqueId> all_owned_fighters{};
        std::vector<EntityUniqueId> fighters{};
    };

    auto data{tests::make_simulation_data(config)};
    data.capital_ships.spawn_delay = 6000.f;
    data.capital_ships.max_health = fighter_ownership_test::collision_resilient_health;
    data.fighters.laser.damage = 0;
    data.fighters.health = fighter_ownership_test::collision_resilient_health;
    for (std::int32_t i{}; i < 4; ++i) {
        tests::add_capital_spawn(data,
                                 Vector3f{{static_cast<float>((i % 2 == 0 ? -1.0 : 1.0) * 250000.0),
                                           static_cast<float>((i / 2) * 200000.0),
                                           0.f}},
                                 i % 2 == 0 ? Team::Green : Team::Red,
                                 i ^ 1,
                                 0.f,
                                 6000.f,
                                 fighter_ownership_test::collision_resilient_health);
    }

    tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    ml::TimeSeriesData<Sample> samples;
    std::vector<EntityUniqueId> killed_capitals{};
    harness.on_end_tick = [&](LevelSim& simulation) {
        auto const& capitals{simulation.get_capital_ships()};
        Sample sample;
        auto const count{capitals.get_num_instances()};
        for (std::int32_t i{}; i < count; ++i) {
            auto const id{capitals.get_id(i)};
            sample.capitals.push_back(id);
            sample.capital_teams.push_back(capitals.get_team(id));
            sample.span_starts.push_back(capitals.get_fighter_id_span(i).start());
            auto const owned{capitals.get_fighter_ids(i)};
            sample.owned_fighters.emplace_back(owned.begin(), owned.end());
            for (auto const fighter : capitals.get_fighter_ids(i)) {
                sample.fighter_teams.push_back(
                    simulation.get_agent_accessor().read_alive(fighter)->team);
            }
        }
        auto const owned{capitals.get_fighter_ids()};
        sample.all_owned_fighters.insert(
            sample.all_owned_fighters.end(), owned.begin(), owned.end());
        auto const live{simulation.get_fighters().get_entity_ids()};
        sample.fighters.insert(sample.fighters.end(), live.begin(), live.end());
        samples.add(harness.get_time(), std::move(sample));
    };
    harness.timeline.at(1.0, [&] {
        auto const& capitals{harness.get_simulation().get_capital_ships()};
        for (auto const team : {Team::Green, Team::Red}) {
            EntityUniqueId victim;
            auto const count{capitals.get_num_instances()};
            for (std::int32_t i{}; i < count; ++i) {
                auto const id{capitals.get_id(i)};
                if (capitals.get_team(id) == team) {
                    victim = id;
                    if (team == Team::Red) {
                        break;
                    }
                }
            }
            if (tests::expect_true(victim.is_valid(), "Team has a capital to kill")) {
                killed_capitals.push_back(victim);
            }
        }
        harness.queue_kills(std::span<EntityUniqueId const>{
            killed_capitals.data(),
            static_cast<std::size_t>(static_cast<std::int32_t>(killed_capitals.size()))});
    });
    harness.timeline.finish_at(2.0);
    tests::expect_true(harness.run_until_timeline_finished(10.0),
                       "Capital reassignment timeline completes");
    tests::expect_true(samples.num() > 1, "Capital reassignment samples are recorded");
    if (::testing::Test::HasFailure()) {
        return;
    }

    auto const& before{samples.value_at(samples.nearest_index(0.5))};
    auto const& after{samples.values().back()};
    tests::expect_equal(
        4, static_cast<std::int32_t>(before.capitals.size()), "Two capitals per team spawn");
    tests::expect_equal(
        2, static_cast<std::int32_t>(after.capitals.size()), "Both killed capitals are removed");
    tests::expect_greater(
        static_cast<std::int32_t>(before.fighters.size()), 0, "One fighter wave spawns");
    tests::expect_equal(static_cast<std::int32_t>(before.fighters.size()),
                        static_cast<std::int32_t>(after.fighters.size()),
                        "All fighters survive");
    std::set<EntityUniqueId> seen;
    std::int32_t offset{};
    auto const count{static_cast<std::int32_t>(after.capitals.size())};
    for (std::int32_t i{}; i < count; ++i) {
        tests::expect_true(!std::ranges::contains(killed_capitals, after.capitals[i]),
                           "Owner capital survives");
        tests::expect_equal(offset, after.span_starts[i], "Ownership spans are contiguous");
        std::int32_t expected_count{};
        auto const before_count{static_cast<std::int32_t>(before.capitals.size())};
        for (std::int32_t j{}; j < before_count; ++j) {
            if (before.capital_teams[j] == after.capital_teams[i]) {
                expected_count += static_cast<std::int32_t>(before.owned_fighters[j].size());
            }
        }
        tests::expect_equal(expected_count,
                            static_cast<std::int32_t>(after.owned_fighters[i].size()),
                            "Survivor owns both original waves on its team");
        for (auto const fighter : after.owned_fighters[i]) {
            tests::expect_true(!std::ranges::contains(seen, fighter),
                               "Fighter ownership is unique");
            seen.insert(fighter);
            tests::expect_true(std::ranges::contains(before.fighters, fighter),
                               "Original fighter is preserved");
            tests::expect_true(std::ranges::contains(after.fighters, fighter),
                               "Owned fighter is in simulation");
            tests::expect_equal(after.capital_teams[i],
                                after.fighter_teams[offset],
                                "Fighter belongs to owner's team");
            if (tests::expect_true((offset >= 0 && static_cast<std::size_t>(offset) <
                                                       after.all_owned_fighters.size()),
                                   "Span is within flat ownership array")) {
                tests::expect_equal(
                    fighter, after.all_owned_fighters[offset], "Span matches flat ownership array");
            }
            ++offset;
        }
    }
    tests::expect_equal(static_cast<std::int32_t>(after.fighters.size()),
                        static_cast<std::int32_t>(seen.size()),
                        "Every fighter has one owner");
    tests::expect_equal(offset,
                        static_cast<std::int32_t>(after.all_owned_fighters.size()),
                        "Spans cover ownership array");
}

/* **************************************** */
// Capital fighter ownership lifecycle
/* **************************************** */
void run_worldless_fighter_ownership(tests::SimulationFixture const& config,
                                     FighterOwnershipScenario const scenario) {
    auto data{tests::make_simulation_data(config)};
    data.capital_ships.spawn_delay = 10.f;
    data.capital_ships.max_health = 10000;
    data.fighters.speed = 2000.f;
    data.fighters.health = fighter_ownership_test::collision_resilient_health;
    data.fighters.laser.damage = 0;
    data.fighters.laser.max_distance = 15000.f;
    auto const green_index{tests::add_capital_spawn(
        data, {{-260800.f, -5060.f, 4360.f}}, Team::Green, 1, 0.f, 6000.f)};
    tests::add_capital_spawn(
        data, {{245260.f, -451630.f, 4360.f}}, Team::Red, green_index, 0.f, 6000.f);
    tests::add_capital_spawn(
        data, {{300450.f, 214000.f, 4360.f}}, Team::Red, green_index, 0.f, 6000.f);

    tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    auto const& capitals{harness.get_simulation().get_capital_ships()};
    auto const& fighters{harness.get_simulation().get_fighters()};
    std::vector<EntityUniqueId> destroyed{};
    std::vector<EntityUniqueId> kept{};
    std::vector<EntityUniqueId> green_fighters_before_capital_kill{};
    auto initial_checked{false};
    auto fighter_kill_checked{scenario == FighterOwnershipScenario::KillCapital};
    auto capital_kill_checked{scenario == FighterOwnershipScenario::KillFightersOnly};

    harness.timeline.at(0.2, [&] {
        tests::expect_equal(3, capitals.get_num_instances(), "Three capitals are registered");
        auto const expected_fighters{3 * capitals.get_fighter_spawn_slots()};
        tests::expect_equal(expected_fighters,
                            fighters.get_num_instances(),
                            "Every capital spawns its fighter slots");
        tests::expect_equal(expected_fighters,
                            static_cast<std::int32_t>(capitals.get_fighter_ids().size()),
                            "Capital-owned and simulation fighter counts match");
        for (std::int32_t i{}; i < capitals.get_num_instances(); ++i) {
            tests::expect_not_equal(
                capitals.get_id(i), capitals.get_target_id(i), "Capital does not target itself", i);
        }
        for (auto const target : fighters.get_target_ids()) {
            tests::expect_true(target.is_valid(), "Spawned fighter has a target");
        }
        initial_checked = true;
    });

    auto next_time{0.4};
    if (scenario != FighterOwnershipScenario::KillCapital) {
        harness.timeline.at(next_time, [&] {
            auto const ids{capitals.get_fighter_ids()};
            auto const count{static_cast<std::int32_t>(ids.size())};
            for (std::int32_t i{}; i < count; ++i) {
                auto const row{
                    harness.get_simulation().get_agent_accessor().indexes().find(ids[i])};
                (i % 2 == 0 ? destroyed : kept).push_back(fighters.get_entity_ids()[row]);
            }
            harness.queue_kills(std::span<EntityUniqueId const>{
                destroyed.data(),
                static_cast<std::size_t>(static_cast<std::int32_t>(destroyed.size()))});
        });
        next_time += 0.2;
        harness.timeline.at(next_time, [&] {
            tests::expect_equal(static_cast<std::int32_t>(kept.size()),
                                fighters.get_num_instances(),
                                "Killed fighters are removed from the simulation");
            tests::expect_equal(static_cast<std::int32_t>(kept.size()),
                                static_cast<std::int32_t>(capitals.get_fighter_ids().size()),
                                "Killed fighters are removed from capital ownership");
            for (auto const id : destroyed) {
                tests::expect_true(!harness.get_simulation().get_agent_accessor().is_alive(id),
                                   "Destroyed fighter is dead");
            }
            fighter_kill_checked = true;
        });
    }

    if (scenario != FighterOwnershipScenario::KillFightersOnly) {
        next_time += 0.2;
        harness.timeline.at(next_time, [&] {
            auto const main_index{capitals.find_first_index_on_team(Team::Green)};
            assert(main_index.has_value());
            auto const owned{capitals.get_fighter_ids(*main_index)};
            green_fighters_before_capital_kill =
                std::vector<EntityUniqueId>{owned.begin(), owned.end()};
            auto const id{capitals.get_target_id(*main_index)};
            auto const row{harness.get_simulation().get_agent_accessor().indexes().find(id)};
            harness.queue_kills(std::array{capitals.get_read_view().entities.entity_ids[row]});
        });
        next_time += 0.5;
        harness.timeline.at(next_time, [&] {
            tests::expect_equal(
                2, capitals.get_num_instances(), "Killed capital is removed from the simulation");
            auto const main_index{capitals.find_first_index_on_team(Team::Green)};
            tests::expect_true(main_index.has_value(), "Green capital survives");
            if (main_index.has_value()) {
                auto const remaining{capitals.get_fighter_ids(*main_index)};
                tests::expect_equal(
                    static_cast<std::int32_t>(green_fighters_before_capital_kill.size()),
                    static_cast<std::int32_t>(remaining.size()),
                    "Surviving capital keeps its fighters");
                for (auto const id : remaining) {
                    tests::expect_true(
                        std::ranges::contains(green_fighters_before_capital_kill, id),
                        "Surviving fighter retains capital ownership");
                    auto const index{
                        harness.get_simulation().get_agent_accessor().indexes().find(id)};
                    tests::expect_true(fighters.get_target_ids()[index].is_valid(),
                                       "Surviving fighter retargets");
                }
            }
            capital_kill_checked = true;
        });
    }
    harness.timeline.finish_at(next_time + 0.1);
    tests::expect_true(harness.run_until_timeline_finished(next_time + 0.2),
                       "Capital fighter ID timeline completes");
    tests::expect_true(initial_checked, "Initial fighter ownership is checked");
    tests::expect_true(fighter_kill_checked, "Fighter removal is checked");
    tests::expect_true(capital_kill_checked, "Capital removal is checked");
}
}
