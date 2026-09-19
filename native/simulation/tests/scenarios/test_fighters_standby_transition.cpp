#include <ioj/sim/capital_ships/sim.h>
#include <ioj/sim/fighters/sim.h>
#include "../support/simulation_test_support.h"

#include "test_fighters_standby_transition.h"

namespace ioj::sim {
namespace fighters_standby_test {
inline constexpr std::int32_t collision_resilient_health{1'000'000};
}

void run_worldless_fighters_standby_transition(tests::SimulationFixture const& config) {
    auto data{tests::make_simulation_data(config)};
    data.fighters.health = fighters_standby_test::collision_resilient_health;
    tests::add_capital_spawn(data,
                             Vector3f{{-4000.f, 0.f, 0.f}},
                             Team::Green,
                             1,
                             0.f,
                             60.f,
                             fighters_standby_test::collision_resilient_health);
    tests::add_capital_spawn(data,
                             Vector3f{{4000.f, 0.f, 0.f}},
                             Team::Red,
                             0,
                             0.f,
                             60.f,
                             fighters_standby_test::collision_resilient_health);

    tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    auto const& capitals{harness.get_simulation().get_capital_ships()};
    auto const& fighters{harness.get_simulation().get_fighters()};
    auto const enemy{capitals.get_id(1)};

    struct Sample {
        std::int32_t capital_count{};
        std::vector<fighters::Sim::Task> tasks{};
        std::vector<Vector3f> velocities{};
        std::vector<EntityUniqueId> parents{};
    };
    ml::TimeSeriesData<Sample> samples;
    harness.on_end_tick = [&](LevelSim&) {
        Sample sample{.capital_count = capitals.get_num_instances()};
        auto const tasks{fighters.get_tasks()};
        sample.tasks.insert(sample.tasks.end(), tasks.begin(), tasks.end());
        auto const parents{fighters.get_parent_ids()};
        sample.parents.insert(sample.parents.end(), parents.begin(), parents.end());
        for (auto const id : fighters.get_entity_ids()) {
            sample.velocities.push_back(
                harness.get_simulation().get_agent_accessor().read(id)->velocity);
        }
        samples.add(harness.get_time(), std::move(sample));
    };
    harness.timeline.then_after(8.0, [&] { harness.queue_kills(std::array{enemy}); })
        .then_after(0.1, [] {})
        .finish_after(0.0);
    tests::expect_true(harness.run_until_timeline_finished(9.0),
                       "Standby-transition timeline completes");
    tests::expect_true(!samples.is_empty(), "Standby-transition samples are recorded");
    if (samples.is_empty()) {
        return;
    }

    auto const& before{samples.nearest_value(8.0)};
    auto const& after{samples.nearest_value(8.1)};
    tests::expect_greater(static_cast<std::int32_t>(before.velocities.size()),
                          std::int32_t{0},
                          "Fighters spawned before kill");
    tests::expect_true(std::ranges::any_of(before.velocities,
                                           [](Vector3f const velocity) {
                                               return (std::abs(velocity.X) > 1.e-4f ||
                                                       std::abs(velocity.Y) > 1.e-4f ||
                                                       std::abs(velocity.Z) > 1.e-4f);
                                           }),
                       "At least one fighter moves before standby");
    tests::expect_equal(1, after.capital_count, "One capital remains after kill");
    tests::expect_equal(static_cast<std::int32_t>(after.tasks.size()),
                        static_cast<std::int32_t>(after.velocities.size()),
                        "Standby tasks and velocities have matching counts");
    std::int32_t standby_fighters{};
    std::int32_t orphaned_fighters{};
    for (std::int32_t i{}; i < static_cast<std::int32_t>(after.tasks.size()); ++i) {
        if (after.parents[i].is_valid()) {
            tests::expect_equal(fighters::Sim::Task::Standby,
                                after.tasks[i],
                                "Owned fighter transitioned to standby",
                                i);
            tests::expect_distance_near(
                after.velocities[i], Vector3f{}, 0.f, "Standby fighter velocity is zero", i);
            ++standby_fighters;
        } else {
            tests::expect_equal(fighters::Sim::Task::Attack,
                                after.tasks[i],
                                "Orphaned fighter retains its attack task",
                                i);
            ++orphaned_fighters;
        }
    }
    tests::expect_greater(standby_fighters, std::int32_t{0}, "Surviving capital retains fighters");
    tests::expect_greater(orphaned_fighters, std::int32_t{0}, "Destroyed capital leaves orphans");
}

}
