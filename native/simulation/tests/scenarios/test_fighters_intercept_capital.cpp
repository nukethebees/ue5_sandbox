#include "test_fighters_intercept_capital.h"
#include "../support/simulation_test_support.h"

#include <ioj/sim/capital_ships/sim.h>
#include <ioj/sim/fighters/sim.h>

#include <algorithm>

namespace ioj::sim {
namespace fighters_intercept_test {
inline constexpr std::int32_t collision_resilient_health{1'000'000};
}

void run_worldless_fighters_intercept_capital(tests::SimulationFixture const& config) {
    auto data{tests::make_simulation_data(config)};
    data.fighters.health = fighters_intercept_test::collision_resilient_health;
    auto const green{tests::add_capital_spawn(data,
                                              {{-61180.f, 2170.f, 4360.f}},
                                              Team::Green,
                                              -1,
                                              0.f,
                                              60.f,
                                              fighters_intercept_test::collision_resilient_health)};
    auto const red{tests::add_capital_spawn(data,
                                            {{77320.f, 2170.f, 4360.f}},
                                            Team::Red,
                                            green,
                                            600.f,
                                            60.f,
                                            fighters_intercept_test::collision_resilient_health)};
    tests::add_capital_spawn(data,
                             {{3590.f, 3240.f, 4360.f}},
                             Team::Blue,
                             green,
                             600.f,
                             60.f,
                             fighters_intercept_test::collision_resilient_health);
    data.level_events.initial_spawns.capital_spawns.get_view().target_entity_indices()[0] = red;

    tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    auto const& capitals{harness.get_simulation().get_capital_ships()};
    auto const& fighters{harness.get_simulation().get_fighters()};
    auto const hero{capitals.get_handle(0)};
    auto const original_target{capitals.get_handle(1)};
    auto const intercept_target{capitals.get_handle(2)};

    struct Sample {
        RegistryEntityHandle parent_target;
        std::vector<RegistryEntityHandle> fighter_targets{};
    };
    ml::TimeSeriesData<Sample> samples;
    harness.on_end_tick = [&](LevelSim&) {
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
    tests::expect_true(harness.run_until_timeline_finished(21.0),
                       "Fighter interception timeline completes");
    tests::expect_true(!samples.is_empty(), "Fighter interception samples are recorded");
    if (samples.is_empty()) {
        return;
    }

    auto const& start{samples.nearest_value(2.0 / 60.0)};
    auto const& end{samples.nearest_value(20.0)};
    tests::expect_greater(static_cast<std::int32_t>(start.fighter_targets.size()),
                          std::int32_t{0},
                          "Parent has fighters");
    tests::expect_greater(static_cast<std::int32_t>(end.fighter_targets.size()),
                          std::int32_t{0},
                          "Parent has fighters at end");
    tests::expect_equal(
        original_target, start.parent_target, "Green capital initially targets red capital");
    for (std::int32_t i{}; i < static_cast<std::int32_t>(start.fighter_targets.size()); ++i) {
        tests::expect_equal(original_target,
                            start.fighter_targets[i],
                            "Initial fighter target matches red parent target",
                            i);
    }
    auto const intercept_count{
        static_cast<std::int32_t>(std::ranges::count(end.fighter_targets, intercept_target))};
    tests::expect_greater(
        intercept_count, std::int32_t{0}, "At least one fighter intercepts the blue capital");
    tests::expect_equal(hero, capitals.get_handle(0), "Hero capital handle remains stable");
}

}
