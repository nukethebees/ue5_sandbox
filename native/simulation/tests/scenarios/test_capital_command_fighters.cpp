#include "test_capital_command_fighters.h"
#include "../support/simulation_test_support.h"

#include <sandbox/simulation/entities/TestEntityRegistry.h>
#include <sandbox/simulation/ships/capital/TestCapitalShipsSimulation.h>

namespace ml {
void run_worldless_capital_command_fighters(ml::simulation_tests::SimulationFixture const& config) {
    using Task = test_capital_ship_fighters::Simulation::Task;
    auto data{ml::simulation_tests::make_simulation_data(config)};
    ml::simulation_tests::add_capital_spawn(
        data, ml::simulation::Vector3f{{-18290.f, 2170.f, 4360.f}}, ml::simulation::Team::Green, 1);
    ml::simulation_tests::add_capital_spawn(
        data, ml::simulation::Vector3f{{17030.f, 2170.f, 4360.f}}, ml::simulation::Team::Red, 0);
    ml::simulation_tests::add_capital_spawn(
        data, ml::simulation::Vector3f{{17030.f, 12170.f, 4360.f}}, ml::simulation::Team::Red, 0);
    ml::simulation_tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    auto const& capitals{harness.get_simulation().get_capital_ships()};
    auto const& fighters{harness.get_simulation().get_capital_ship_fighters()};
    auto const first_target{capitals.get_handle(1)};
    FRegistryEntityHandle second_target;
    harness.timeline
        .then_after(2.0 / 60.0,
                    [&] {
                        ml::simulation_tests::expect_equal(
                            first_target,
                            capitals.get_target_handle(0),
                            "Capital initially retains its configured target");
                        ml::simulation_tests::expect_greater(
                            static_cast<std::int32_t>(capitals.get_fighter_handles(0).size()),
                            std::int32_t{0},
                            "Main capital spawned fighters");
                        harness.queue_kills(std::array{first_target});
                    })
        .then_after(2.0 / 60.0,
                    [&] {
                        second_target = capitals.get_target_handle(0);
                        ml::simulation_tests::expect_true(
                            second_target.is_valid() && second_target != first_target,
                            "Capital retargets after its first target dies");
                        for (auto const fighter_handle : capitals.get_fighter_handles(0)) {
                            ml::simulation_tests::expect_equal(
                                second_target,
                                fighters.get_target_handle(fighter_handle),
                                "Fighter follows the replacement capital target");
                        }
                        auto const enemies{harness.get_registry().get_handles_not_in_team(
                            ml::simulation::Team::Green)};
                        harness.queue_kills(enemies);
                    })
        .then_after(2.0 / 60.0,
                    [&] {
                        for (auto const task : fighters.get_tasks()) {
                            ml::simulation_tests::expect_equal(
                                Task::Standby, task, "Fighter stands by with no enemies");
                        }
                        ml::simulation_tests::expect_greater(capitals.get_num_instances(),
                                                             std::int32_t{0},
                                                             "The main capital remains alive");
                    })
        .finish_after(0.0);
    ml::simulation_tests::expect_true(harness.run_until_timeline_finished(1.0),
                                      "Capital fighter-command timeline completes");
}

}
