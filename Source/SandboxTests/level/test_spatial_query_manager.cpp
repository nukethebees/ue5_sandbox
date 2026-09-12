#include <SpaceGame/entities/TestEntity.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGameSimulation/simulation/SpatialQueryManager.h>

#include <SandboxCoreEngine/actor_utils.h>

#include <Misc/Optional.h>
#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/WorldlessSimulationTest.h>
#include "test_spatial_query_manager.h"

namespace ml {
void run_worldless_spatial_query_line_of_sight(FAutomationTestBase& test,
                                               FSoftTestAssertions& checks,
                                               USpaceGameLevelConfig const& config) {
    constexpr float distance{30000.f};
    TArray<FVector3f> const locations{
        {0.f, distance, 0.f}, {0.f, -distance, 0.f}, {distance, 0.f, 0.f}, {-distance, 0.f, 0.f}};
    auto data{make_worldless_simulation_test_data(config)};
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_ships.fighter_spawn_slots_relative_transforms.Reset();
    for (auto const location : locations) {
        add_worldless_capital_spawn(data, location, ETestTeam::White, INDEX_NONE, 999.f, 999.f);
    }
    FWorldlessSimulationTest harness{MoveTemp(data)};
    harness.finish_initialisation();
    auto const& capitals{harness.get_simulation().get_capital_ships()};
    TArray<FRegistryEntityHandle> expected;
    for (int32 i{}; i < locations.Num(); ++i) {
        expected.Add(capitals.get_handle(i));
    }

    FVectors3f starts;
    FVectors3f ends;
    TArray<FRegistryEntityHandle> targets;
    TStaticArray<float, 3> const scales{0.5f, 1.f, 2.f};
    for (auto const scale : scales) {
        for (int32 i{}; i < locations.Num(); ++i) {
            starts.add(FVector3f::ZeroVector);
            ends.add(locations[i] * scale);
            targets.Add(expected[i]);
        }
    }
    TArray<FRegistryEntityHandle> results;
    results.SetNumUninitialized(ends.num());
    harness.get_simulation().get_spatial_query_manager().trace_line_of_sight(
        starts.get_const_view(), ends.get_const_view(), results);
    auto const count{locations.Num()};
    for (int32 i{}; i < count; ++i) {
        checks.is_true(results[i].is_null(), TEXT("Half-distance trace misses"), i);
        checks.are_equal(expected[i], results[i + count], TEXT("Ship trace resolves handle"), i);
        checks.are_equal(
            expected[i], results[i + 2 * count], TEXT("Past-ship trace resolves handle"), i);
    }

    TArray<uint8> has_los;
    has_los.SetNumUninitialized(ends.num());
    harness.get_simulation().get_spatial_query_manager().has_line_of_sight_to_targets(
        FVector3f::ZeroVector, ends.get_const_view(), targets, has_los);
    for (int32 i{}; i < has_los.Num(); ++i) {
        checks.are_equal(uint8{1}, has_los[i], TEXT("Clear or target hit has line of sight"), i);
    }
    for (int32 i{}; i < count; ++i) {
        auto const other{expected[(i + 1) % count]};
        targets[i + count] = other;
        targets[i + 2 * count] = other;
    }
    harness.get_simulation().get_spatial_query_manager().has_line_of_sight_to_targets(
        FVector3f::ZeroVector, ends.get_const_view(), targets, has_los);
    for (int32 i{}; i < count; ++i) {
        checks.are_equal(uint8{1}, has_los[i], TEXT("Clear line remains visible"), i);
        checks.are_equal(uint8{0}, has_los[i + count], TEXT("Other target is blocked"), i);
        checks.are_equal(
            uint8{0}, has_los[i + 2 * count], TEXT("Other target past hit is blocked"), i);
    }
    test.TestTrue(TEXT("Line-of-sight query batch completed"), true);
}

}
