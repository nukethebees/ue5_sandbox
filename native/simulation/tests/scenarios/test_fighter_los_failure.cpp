#include "test_fighter_los_failure.h"
#include "../support/simulation_test_support.h"

#include <ioj/sim/capital_ships/sim.h>
#include <ioj/sim/fighters/sim.h>

namespace ioj::sim {
void run_worldless_fighter_los_failure(tests::SimulationFixture const& config) {
    auto data{tests::make_simulation_data(config)};
    auto const blue{
        tests::add_capital_spawn(data, {{-39600.f, 2170.f, 4360.f}}, Team::Blue, -1, 0.f, 1000.f)};
    auto const red{tests::add_capital_spawn(
        data, {{50180.f, 2170.f, 4360.f}}, Team::Red, blue, 10000.f, 10000.f)};
    data.level_events.initial_spawns.capital_spawns.get_view().target_entity_indices()[0] = red;

    tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    auto const& capitals{harness.get_simulation().get_capital_ships()};
    auto const& fighters{harness.get_simulation().get_fighters()};
    auto const enemy{capitals.get_id(1)};
    auto const initial_enemy_health{capitals.get_health(enemy)};
    std::vector<std::vector<Team>> fighter_team_samples{};
    harness.on_end_tick = [&](LevelSim&) {
        std::vector<Team> teams{};
        for (auto const team : fighters.get_teams()) {
            teams.push_back(team);
        }
        fighter_team_samples.push_back(std::move(teams));
    };
    harness.timeline.finish_at(30.0);
    tests::expect_true(harness.run_until_timeline_finished(31.0),
                       "Fighter line-of-sight failure timeline completes");
    tests::expect_true(!fighter_team_samples.empty(), "Simulation produced samples");
    for (auto const& teams : fighter_team_samples) {
        tests::expect_true(std::ranges::all_of(teams, [](auto team) { return team == Team::Blue; }),
                           "Only the blue hero team has fighters");
    }
    tests::expect_greater(initial_enemy_health,
                          capitals.get_health(enemy),
                          "Enemy capital has sustained damage by the end of the test");
}

}
