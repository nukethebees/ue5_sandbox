#include <SpaceGameSimulation/combat/lasers/TestLasersSimulation.h>
#include <SpaceGameSimulation/defences/spinners/TestTubeSpinnersSimulation.h>
#include <SpaceGameSimulation/entities/TestEntityRegistry.h>
#include <SpaceGameSimulation/simulation/SimulationClock.h>
#include <SpaceGameSimulation/simulation/SpatialQueryManager.h>

#include <SandboxCore/frame_memory_resource.h>
#include <SandboxCore/soa_vector_utils.h>

#include <CQTest.h>

namespace ml::test_tube_spinners {
struct FSpinnerSpawnTestAccess {
    static void spawn(Simulation& simulation,
                      FVectors3f::ConstView locations,
                      TConstArrayView<float> yaws,
                      TConstArrayView<int32> fire_points) {
        simulation.spawn_instances(locations, yaws, fire_points);
    }
    static auto entities(Simulation& simulation) -> EntityData& { return simulation.entities; }
};
}

TEST_CLASS(SpinnerSpawning, "Sandbox.UnitTests")
{
    TEST_METHOD(RepeatedAppendsPreserveRowsAndCooldowns)
    {
        using Access = ml::test_tube_spinners::FSpinnerSpawnTestAccess;
        FSimulationClock clock;
        FTestEntityRegistry registry;
        ml::FSpatialQueryManager queries{registry};
        ml::FFrameMemoryResource frame_memory{1024 * 1024};
        ml::test_lasers::Simulation lasers{clock, registry, queries, frame_memory};
        ml::test_tube_spinners::Simulation simulation{clock, registry, lasers, frame_memory};
        simulation.entity_radius = 17.f;
        auto& entities{Access::entities(simulation)};
        entities.laser_cooldowns.set_tick_value(23);

        FVectors3f locations;
        locations.add_defaulted(3);
        locations.set(0, FVector3f{1.f, 2.f, 3.f});
        locations.set(1, FVector3f{4.f, 5.f, 6.f});
        locations.set(2, FVector3f{7.f, 8.f, 9.f});
        TArray<float> const yaws{10.f, 20.f, 30.f};
        TArray<int32> const fire_points{2, 0, 1};
        Access::spawn(simulation,
                      locations.left(1).get_const_view(),
                      MakeArrayView(yaws).Left(1),
                      MakeArrayView(fire_points).Left(1));
        auto const first_handle{entities.handles[0]};
        entities.laser_cooldowns.restart_counter(0);
        entities.laser_cooldowns.tick();
        auto const first_cooldown{entities.laser_cooldowns.get_view()[0]};

        Access::spawn(simulation,
                      locations.right(2).get_const_view(),
                      MakeArrayView(yaws).Right(2),
                      MakeArrayView(fire_points).Right(2));
        auto const second_handle{entities.handles[1]};
        entities.laser_cooldowns.restart_counter(1);
        Access::spawn(simulation, locations.get_const_view(), yaws, fire_points);
        Access::spawn(simulation,
                      locations.left(0).get_const_view(),
                      MakeArrayView(yaws).Left(0),
                      MakeArrayView(fire_points).Left(0));

        entities.validate_array_sizes();
        ASSERT_THAT(AreEqual(6, simulation.get_num_instances()));
        ASSERT_THAT(IsTrue(first_handle == entities.handles[0]));
        ASSERT_THAT(IsTrue(second_handle == entities.handles[1]));
        ASSERT_THAT(AreEqual(first_cooldown, entities.laser_cooldowns.get_view()[0]));
        ASSERT_THAT(AreEqual(int16{23}, entities.laser_cooldowns.get_view()[1]));
        auto const count{entities.num()};
        for (int32 i{}; i < count; ++i) {
            auto const source_index{i % 3};
            ASSERT_THAT(IsTrue(yaws[source_index] == entities.yaws[i]));
            ASSERT_THAT(AreEqual(fire_points[source_index], entities.next_fire_point_indices[i]));
            ASSERT_THAT(IsTrue(locations.xs[source_index] == entities.locations.xs[i]));
            ASSERT_THAT(IsTrue(locations.ys[source_index] == entities.locations.ys[i]));
            ASSERT_THAT(IsTrue(locations.zs[source_index] == entities.locations.zs[i]));
            ASSERT_THAT(IsTrue(registry.is_valid_alive(entities.handles[i])));
            ASSERT_THAT(AreEqual(ETestTeam::White, registry.get_team(entities.handles[i])));
            for (int32 j{}; j < i; ++j) {
                ASSERT_THAT(IsTrue(entities.handles[i] != entities.handles[j]));
            }
            if (i >= 2) {
                ASSERT_THAT(AreEqual(int16{0}, entities.laser_cooldowns.get_view()[i]));
            }
        }
        entities.laser_cooldowns.restart_counter(5);
        ASSERT_THAT(AreEqual(int16{23}, entities.laser_cooldowns.get_view()[5]));
    }
};
