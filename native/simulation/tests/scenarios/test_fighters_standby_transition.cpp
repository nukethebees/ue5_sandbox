#include <sandbox/simulation/entities/TestEntityRegistry.h>
#include <sandbox/simulation/ships/capital/TestCapitalShipsSimulation.h>
#include <sandbox/simulation/ships/fighters/TestCapitalShipFightersSimulation.h>
#include "../support/simulation_test_support.h"

#include "test_fighters_standby_transition.h"

namespace ml {
namespace fighters_standby_test {
inline constexpr std::int32_t collision_resilient_health{1'000'000};
}

void run_worldless_fighters_standby_transition(
    ml::simulation_tests::SimulationFixture const& config) {
    auto data{ml::simulation_tests::make_simulation_data(config)};
    data.fighters.health = fighters_standby_test::collision_resilient_health;
    ml::simulation_tests::add_capital_spawn(data,
                                            ml::simulation::Vector3f{{-4000.f, 0.f, 0.f}},
                                            ml::simulation::Team::Green,
                                            1,
                                            0.f,
                                            60.f,
                                            fighters_standby_test::collision_resilient_health);
    ml::simulation_tests::add_capital_spawn(data,
                                            ml::simulation::Vector3f{{4000.f, 0.f, 0.f}},
                                            ml::simulation::Team::Red,
                                            0,
                                            0.f,
                                            60.f,
                                            fighters_standby_test::collision_resilient_health);

    ml::simulation_tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    auto const& capitals{harness.get_simulation().get_capital_ships()};
    auto const& fighters{harness.get_simulation().get_capital_ship_fighters()};
    auto const enemy{capitals.get_handle(1)};

    struct Sample {
        std::int32_t capital_count{};
        std::vector<test_capital_ship_fighters::Simulation::Task> tasks{};
        std::vector<ml::simulation::Vector3f> velocities{};
    };
    TimeSeriesData<Sample> samples;
    harness.on_end_tick = [&](FLevelSimulation&) {
        Sample sample{.capital_count = capitals.get_num_instances()};
        auto const tasks{fighters.get_tasks()};
        sample.tasks.insert(sample.tasks.end(), tasks.begin(), tasks.end());
        for (auto const handle : fighters.get_handles()) {
            sample.velocities.push_back(harness.get_registry().get_velocity(handle));
        }
        samples.add(harness.get_time(), std::move(sample));
    };
    harness.timeline.then_after(8.0, [&] { harness.queue_kills(std::array{enemy}); })
        .then_after(0.1, [] {})
        .finish_after(0.0);
    ml::simulation_tests::expect_true(harness.run_until_timeline_finished(9.0),
                                      "Standby-transition timeline completes");
    ml::simulation_tests::expect_true(!samples.is_empty(),
                                      "Standby-transition samples are recorded");
    if (samples.is_empty()) {
        return;
    }

    auto const& before{samples.nearest_value(8.0)};
    auto const& after{samples.nearest_value(8.1)};
    ml::simulation_tests::expect_greater(static_cast<std::int32_t>(before.velocities.size()),
                                         std::int32_t{0},
                                         "Fighters spawned before kill");
    ml::simulation_tests::expect_true(
        std::ranges::any_of(before.velocities,
                            [](ml::simulation::Vector3f const velocity) {
                                return (std::abs(velocity.X) > 1.e-4f ||
                                        std::abs(velocity.Y) > 1.e-4f ||
                                        std::abs(velocity.Z) > 1.e-4f);
                            }),
        "At least one fighter moves before standby");
    ml::simulation_tests::expect_equal(1, after.capital_count, "One capital remains after kill");
    ml::simulation_tests::expect_equal(static_cast<std::int32_t>(after.tasks.size()),
                                       static_cast<std::int32_t>(after.velocities.size()),
                                       "Standby tasks and velocities have matching counts");
    for (std::int32_t i{}; i < static_cast<std::int32_t>(after.tasks.size()); ++i) {
        ml::simulation_tests::expect_equal(test_capital_ship_fighters::Simulation::Task::Standby,
                                           after.tasks[i],
                                           "Fighter transitioned to standby",
                                           i);
        ml::simulation_tests::expect_distance_near(after.velocities[i],
                                                   ml::simulation::Vector3f{},
                                                   0.f,
                                                   "Standby fighter velocity is zero",
                                                   i);
    }
}

}
