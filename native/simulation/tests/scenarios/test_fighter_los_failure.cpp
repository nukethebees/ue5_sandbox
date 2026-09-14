#include "test_fighter_los_failure.h"
#include "../support/simulation_test_support.h"

#include <ioj/sim/capital_ships/sim.h>
#include <ioj/sim/fighters/sim.h>

namespace ioj::sim {
void run_worldless_fighter_los_failure(ioj::sim::tests::SimulationFixture const& config) {
    auto data{ioj::sim::tests::make_simulation_data(config)};
    data.capital_spawns.add_defaulted(2);
    data.capital_spawns.locations.xs = {-39600.f, 50180.f};
    data.capital_spawns.locations.ys = {2170.f, 2170.f};
    data.capital_spawns.locations.zs = {4360.f, 4360.f};
    data.capital_spawns.teams = {ioj::sim::Team::Blue, ioj::sim::Team::Red};
    data.capital_spawns.healths = {data.capital_ships.max_health, data.capital_ships.max_health};
    data.capital_spawns.initial_spawn_delays = {0.f, 10000.f};
    data.capital_spawns.spawn_cooldowns = {1000.f, 10000.f};
    data.capital_target_spawn_indices = {1, 0};

    ioj::sim::tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    auto const& capitals{harness.get_simulation().get_capital_ships()};
    auto const& fighters{harness.get_simulation().get_fighters()};
    auto const enemy{capitals.get_handle(1)};
    auto const initial_enemy_health{capitals.get_health(enemy)};
    std::vector<std::vector<ioj::sim::Team>> fighter_team_samples{};
    harness.on_end_tick = [&](LevelSim&) {
        std::vector<ioj::sim::Team> teams{};
        for (auto const team : fighters.get_teams()) {
            teams.push_back(team);
        }
        fighter_team_samples.push_back(std::move(teams));
    };
    harness.timeline.finish_at(30.0);
    ioj::sim::tests::expect_true(harness.run_until_timeline_finished(31.0),
                                 "Fighter line-of-sight failure timeline completes");
    ioj::sim::tests::expect_true(!fighter_team_samples.empty(), "Simulation produced samples");
    for (auto const& teams : fighter_team_samples) {
        ioj::sim::tests::expect_true(
            std::ranges::all_of(teams, [](auto team) { return team == ioj::sim::Team::Blue; }),
            "Only the blue hero team has fighters");
    }
    ioj::sim::tests::expect_greater(initial_enemy_health,
                                    capitals.get_health(enemy),
                                    "Enemy capital has sustained damage by the end of the test");
}

}
