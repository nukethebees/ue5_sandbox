#include "test_fighters_intercept_capital.h"
#include "../support/simulation_test_support.h"

#include <sandbox/simulation/ships/capital/TestCapitalShipsSimulation.h>
#include <sandbox/simulation/ships/fighters/TestCapitalShipFightersSimulation.h>

namespace ml {
namespace fighters_intercept_test {
inline constexpr std::int32_t collision_resilient_health{1'000'000};
}

void run_worldless_fighters_intercept_capital(
    ml::simulation_tests::SimulationFixture const& config) {
    auto data{ml::simulation_tests::make_simulation_data(config)};
    data.fighters.health = fighters_intercept_test::collision_resilient_health;
    data.capital_spawns.add_defaulted(3);
    data.capital_spawns.locations.xs = {-61180.f, 77320.f, 3590.f};
    data.capital_spawns.locations.ys = {2170.f, 2170.f, 3240.f};
    data.capital_spawns.locations.zs = {4360.f, 4360.f, 4360.f};
    data.capital_spawns.teams = {
        ml::simulation::Team::Green, ml::simulation::Team::Red, ml::simulation::Team::Blue};
    data.capital_spawns.healths.assign(3, fighters_intercept_test::collision_resilient_health);
    data.capital_spawns.initial_spawn_delays = {0.f, 600.f, 600.f};
    data.capital_spawns.spawn_cooldowns = {60.f, 60.f, 60.f};
    data.capital_target_spawn_indices = {1, 0, 0};

    ml::simulation_tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    auto const& capitals{harness.get_simulation().get_capital_ships()};
    auto const& fighters{harness.get_simulation().get_capital_ship_fighters()};
    auto const hero{capitals.get_handle(0)};
    auto const original_target{capitals.get_handle(1)};
    auto const intercept_target{capitals.get_handle(2)};

    struct Sample {
        FRegistryEntityHandle parent_target;
        std::vector<FRegistryEntityHandle> fighter_targets{};
    };
    TimeSeriesData<Sample> samples;
    harness.on_end_tick = [&](FLevelSimulation&) {
        Sample sample;
        sample.parent_target = capitals.get_target_handle(0);
        for (auto const fighter_handle : capitals.get_fighter_handles(0)) {
            if (fighters.has_handle(fighter_handle)) {
                sample.fighter_targets.push_back(fighters.get_target_handle(fighter_handle));
            }
        }
        samples.add(harness.get_time(), std::move(sample));
    };
    harness.timeline.finish_at(20.0);
    ml::simulation_tests::expect_true(harness.run_until_timeline_finished(21.0),
                                      "Fighter interception timeline completes");
    ml::simulation_tests::expect_true(!samples.is_empty(),
                                      "Fighter interception samples are recorded");
    if (samples.is_empty()) {
        return;
    }

    auto const& start{samples.nearest_value(2.0 / 60.0)};
    auto const& end{samples.nearest_value(20.0)};
    ml::simulation_tests::expect_greater(static_cast<std::int32_t>(start.fighter_targets.size()),
                                         std::int32_t{0},
                                         "Parent has fighters");
    ml::simulation_tests::expect_greater(static_cast<std::int32_t>(end.fighter_targets.size()),
                                         std::int32_t{0},
                                         "Parent has fighters at end");
    ml::simulation_tests::expect_equal(
        original_target, start.parent_target, "Green capital initially targets red capital");
    for (std::int32_t i{}; i < static_cast<std::int32_t>(start.fighter_targets.size()); ++i) {
        ml::simulation_tests::expect_equal(original_target,
                                           start.fighter_targets[i],
                                           "Initial fighter target matches red parent target",
                                           i);
    }
    for (std::int32_t i{}; i < static_cast<std::int32_t>(end.fighter_targets.size()); ++i) {
        ml::simulation_tests::expect_equal(
            intercept_target, end.fighter_targets[i], "Final fighter target is blue capital", i);
    }
    ml::simulation_tests::expect_equal(
        hero, capitals.get_handle(0), "Hero capital handle remains stable");
}

}
