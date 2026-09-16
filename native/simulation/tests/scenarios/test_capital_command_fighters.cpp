#include "test_capital_command_fighters.h"
#include "../support/simulation_test_support.h"

#include <ioj/sim/capital_ships/sim.h>
#include <ioj/sim/entity_registry.h>

namespace ioj::sim {
void run_worldless_capital_command_fighters(tests::SimulationFixture const& config) {
    using Task = fighters::Sim::Task;
    auto data{tests::make_simulation_data(config)};
    tests::add_capital_spawn(data, Vector3f{{-18290.f, 2170.f, 4360.f}}, Team::Green, 1);
    tests::add_capital_spawn(data, Vector3f{{17030.f, 2170.f, 4360.f}}, Team::Red, 0);
    tests::add_capital_spawn(data, Vector3f{{17030.f, 12170.f, 4360.f}}, Team::Red, 0);
    tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    auto const& capitals{harness.get_simulation().get_capital_ships()};
    auto const& fighters{harness.get_simulation().get_fighters()};
    auto const first_target{capitals.get_handle(1)};
    EntityUniqueId second_target;
    SimTick final_kill_tick{};
    harness.on_end_tick = [&](LevelSim& simulation) {
        if (final_kill_tick != 0 &&
            simulation.get_clock().get_completed_ticks() == final_kill_tick + 2) {
            for (auto const task : fighters.get_tasks()) {
                tests::expect_equal(
                    Task::Attack, task, "Capital orders do not change effective tasks in Thinking");
            }
        }
    };
    harness.timeline
        .then_after(2.0 / 60.0,
                    [&] {
                        tests::expect_equal(harness.get_registry().get_current_id(first_target),
                                            capitals.get_target_id(0),
                                            "Capital initially retains its configured target");
                        tests::expect_greater(
                            static_cast<std::int32_t>(capitals.get_fighter_ids(0).size()),
                            std::int32_t{0},
                            "Main capital spawned fighters");
                        harness.queue_kills(std::array{first_target});
                    })
        .then_after(
            3.0 / 60.0,
            [&] {
                second_target = capitals.get_target_id(0);
                tests::expect_true(second_target.is_valid() &&
                                       second_target !=
                                           harness.get_registry().get_current_id(first_target),
                                   "Capital retargets after its first target dies");
                for (auto const fighter_id : capitals.get_fighter_ids(0)) {
                    auto const index{
                        harness.get_simulation().get_agent_accessor().indexes().find(fighter_id)};
                    tests::expect_equal(second_target,
                                        fighters.get_target_ids()[index],
                                        "Fighter follows the replacement capital target");
                }
                auto const enemies{harness.get_registry().get_handles_not_in_team(Team::Green)};
                harness.queue_kills(enemies);
                final_kill_tick = harness.get_simulation().get_clock().get_completed_ticks();
            })
        .then_after(3.0 / 60.0,
                    [&] {
                        for (auto const task : fighters.get_tasks()) {
                            tests::expect_equal(
                                Task::Standby, task, "Fighter stands by with no enemies");
                        }
                        tests::expect_greater(capitals.get_num_instances(),
                                              std::int32_t{0},
                                              "The main capital remains alive");
                    })
        .finish_after(0.0);
    tests::expect_true(harness.run_until_timeline_finished(1.0),
                       "Capital fighter-command timeline completes");
}

}
