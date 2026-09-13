#include <sandbox/core/tick_countdown.h>
#include <sandbox/simulation/combat/lasers/TestLasersSimulation.h>
#include <sandbox/simulation/defences/spinners/TestTubeSpinnersSimulation.h>
#include <sandbox/simulation/entities/TestEntityRegistry.h>
#include <sandbox/simulation/simulation/SimulationClock.h>
#include <sandbox/simulation/simulation/SpatialQueryManager.h>
#include "support/simulation_test_support.h"

namespace ml::test_tube_spinners {
struct FSpinnerSpawnTestAccess {
    static void spawn(Simulation& simulation,
                      ml::simulation::Vectors3f::ConstView locations,
                      std::span<float const> yaws,
                      std::span<std::int32_t const> fire_points) {
        simulation.spawn_instances(
            locations,
            std::span<float const>{
                yaws.data(), static_cast<std::size_t>(static_cast<std::int32_t>(yaws.size()))},
            std::span<std::int32_t const>{
                fire_points.data(),
                static_cast<std::size_t>(static_cast<std::int32_t>(fire_points.size()))});
    }
    static auto entities(Simulation& simulation) -> ml::simulation::SpinnerEntityData& {
        return simulation.entities;
    }
    static void tick_cooldowns(Simulation& simulation) { simulation.update_timers(0.f); }
    static void set_cooldown(Simulation& simulation, std::int16_t const ticks) {
        simulation.cooldown_restart_ticks_ = ticks;
    }
    static auto cooldowns(Simulation& simulation) -> ml::TickCountdownView<std::int16_t> {
        return {simulation.entities.laser_cooldowns, simulation.cooldown_restart_ticks_};
    }
};
}

TEST(SpinnerSpawning, RepeatedAppendsPreserveRowsAndCooldowns) {

    using Access = ml::test_tube_spinners::FSpinnerSpawnTestAccess;
    FSimulationClock clock;
    FTestEntityRegistry registry;
    ml::FSpatialQueryManager queries{registry};
    ml::FrameMemoryResource frame_memory{1024 * 1024};
    ml::test_lasers::Simulation lasers{clock, registry, queries, frame_memory};
    ml::test_tube_spinners::Simulation simulation{clock, registry, lasers, frame_memory};
    simulation.entity_radius = 17.f;
    auto& entities{Access::entities(simulation)};
    Access::set_cooldown(simulation, 23);

    ml::simulation::Vectors3f locations;
    locations.add_defaulted(3);
    locations.set(0, ml::simulation::Vector3f{{1.f, 2.f, 3.f}});
    locations.set(1, ml::simulation::Vector3f{{4.f, 5.f, 6.f}});
    locations.set(2, ml::simulation::Vector3f{{7.f, 8.f, 9.f}});
    std::vector<float> const yaws{10.f, 20.f, 30.f};
    std::vector<std::int32_t> const fire_points{2, 0, 1};
    Access::spawn(simulation,
                  locations.left(1).get_const_view(),
                  std::span{yaws}.first(1),
                  std::span{fire_points}.first(1));
    auto const first_handle{entities.handles[0]};
    Access::cooldowns(simulation).restart_counter(0);
    Access::tick_cooldowns(simulation);
    auto const first_cooldown{entities.laser_cooldowns[0]};

    Access::spawn(simulation,
                  locations.right(2).get_const_view(),
                  std::span{yaws}.last(2),
                  std::span{fire_points}.last(2));
    auto const second_handle{entities.handles[1]};
    Access::cooldowns(simulation).restart_counter(1);
    Access::spawn(simulation, locations.get_const_view(), yaws, fire_points);
    Access::spawn(simulation,
                  locations.left(0).get_const_view(),
                  std::span{yaws}.first(0),
                  std::span{fire_points}.first(0));

    entities.validate_array_sizes();
    ASSERT_EQ((6), (simulation.get_num_instances()));
    ASSERT_TRUE((first_handle == entities.handles[0]));
    ASSERT_TRUE((second_handle == entities.handles[1]));
    ASSERT_EQ((first_cooldown), (entities.laser_cooldowns[0]));
    ASSERT_EQ((std::int16_t{23}), (entities.laser_cooldowns[1]));
    auto const count{entities.num()};
    for (std::int32_t i{}; i < count; ++i) {
        auto const source_index{i % 3};
        ASSERT_TRUE((yaws[source_index] == entities.yaws[i]));
        ASSERT_EQ((fire_points[source_index]), (entities.next_fire_point_indices[i]));
        ASSERT_TRUE((locations.xs[source_index] == entities.locations.xs[i]));
        ASSERT_TRUE((locations.ys[source_index] == entities.locations.ys[i]));
        ASSERT_TRUE((locations.zs[source_index] == entities.locations.zs[i]));
        ASSERT_TRUE((registry.is_valid_alive(entities.handles[i])));
        ASSERT_TRUE((ml::simulation::Team::White == registry.get_team(entities.handles[i])));
        for (std::int32_t j{}; j < i; ++j) {
            ASSERT_TRUE((entities.handles[i] != entities.handles[j]));
        }
        if (i >= 2) {
            ASSERT_EQ((std::int16_t{0}), (entities.laser_cooldowns[i]));
        }
    }
    Access::cooldowns(simulation).restart_counter(5);
    ASSERT_EQ((std::int16_t{23}), (entities.laser_cooldowns[5]));
}
