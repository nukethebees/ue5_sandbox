#include "test_capital_command_fighters.h"
#include "../support/simulation_test_support.h"

#include <ioj/sim/capital_ships/sim.h>
#include <ioj/sim/entity_registry.h>

namespace ioj::sim {
void run_worldless_capital_command_fighters(ioj::sim::tests::SimulationFixture const& config) {
    using Task = fighters::Sim::Task;
    auto data{ioj::sim::tests::make_simulation_data(config)};
    ioj::sim::tests::add_capital_spawn(
        data, ioj::sim::Vector3f{{-18290.f, 2170.f, 4360.f}}, ioj::sim::Team::Green, 1);
    ioj::sim::tests::add_capital_spawn(
        data, ioj::sim::Vector3f{{17030.f, 2170.f, 4360.f}}, ioj::sim::Team::Red, 0);
    ioj::sim::tests::add_capital_spawn(
        data, ioj::sim::Vector3f{{17030.f, 12170.f, 4360.f}}, ioj::sim::Team::Red, 0);
    ioj::sim::tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    auto const& capitals{harness.get_simulation().get_capital_ships()};
    auto const& fighters{harness.get_simulation().get_fighters()};
    auto const first_target{capitals.get_handle(1)};
    RegistryEntityHandle second_target;
    harness.timeline
        .then_after(2.0 / 60.0,
                    [&] {
                        ioj::sim::tests::expect_equal(
                            first_target,
                            capitals.get_target_handle(0),
                            "Capital initially retains its configured target");
                        ioj::sim::tests::expect_greater(
                            static_cast<std::int32_t>(capitals.get_fighter_handles(0).size()),
                            std::int32_t{0},
                            "Main capital spawned fighters");
                        harness.queue_kills(std::array{first_target});
                    })
        .then_after(
            2.0 / 60.0,
            [&] {
                second_target = capitals.get_target_handle(0);
                ioj::sim::tests::expect_true(second_target.is_valid() &&
                                                 second_target != first_target,
                                             "Capital retargets after its first target dies");
                for (auto const fighter_handle : capitals.get_fighter_handles(0)) {
                    ioj::sim::tests::expect_equal(second_target,
                                                  fighters.get_target_handle(fighter_handle),
                                                  "Fighter follows the replacement capital target");
                }
                auto const enemies{
                    harness.get_registry().get_handles_not_in_team(ioj::sim::Team::Green)};
                harness.queue_kills(enemies);
            })
        .then_after(2.0 / 60.0,
                    [&] {
                        for (auto const task : fighters.get_tasks()) {
                            ioj::sim::tests::expect_equal(
                                Task::Standby, task, "Fighter stands by with no enemies");
                        }
                        ioj::sim::tests::expect_greater(capitals.get_num_instances(),
                                                        std::int32_t{0},
                                                        "The main capital remains alive");
                    })
        .finish_after(0.0);
    ioj::sim::tests::expect_true(harness.run_until_timeline_finished(1.0),
                                 "Capital fighter-command timeline completes");
}

}
