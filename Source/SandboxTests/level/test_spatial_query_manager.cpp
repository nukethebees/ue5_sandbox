#include <array>
#include <sandbox/simulation/simulation/SpatialQueryManager.h>
#include <SpaceGame/entities/TestEntity.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGameSimulation/simulation/NativeVectorTypes.h>
#include <vector>

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
    data.capital_ships.fighter_spawn_slots_relative_transforms.clear();
    for (auto const location : locations) {
        add_worldless_capital_spawn(data, location, ETestTeam::White, INDEX_NONE, 999.f, 999.f);
    }
    FWorldlessSimulationTest harness{MoveTemp(data)};
    harness.finish_initialisation();
    auto const& capitals{harness.get_simulation().get_capital_ships()};
    std::vector<FRegistryEntityHandle> expected;
    for (int32 i{}; i < locations.Num(); ++i) {
        expected.push_back(capitals.get_handle(i));
    }

    ml::simulation::Vectors3f starts;
    ml::simulation::Vectors3f ends;
    std::vector<FRegistryEntityHandle> targets;
    TStaticArray<float, 3> const scales{0.5f, 1.f, 2.f};
    for (auto const scale : scales) {
        for (int32 i{}; i < locations.Num(); ++i) {
            starts.add(ml::make_vector3f(0.f, 0.f, 0.f));
            ends.add(ml::to_native(locations[i] * scale));
            targets.push_back(expected[i]);
        }
    }
    std::vector<FRegistryEntityHandle> results;
    results.resize(static_cast<std::size_t>(ends.num()));
    harness.get_simulation().get_spatial_query_manager().trace_line_of_sight(
        starts.get_const_view(), ends.get_const_view(), results);
    auto const count{locations.Num()};
    for (int32 i{}; i < count; ++i) {
        checks.is_true(results[i].is_null(), TEXT("Half-distance trace misses"), i);
        checks.are_equal(expected[i], results[i + count], TEXT("Ship trace resolves handle"), i);
        checks.are_equal(
            expected[i], results[i + 2 * count], TEXT("Past-ship trace resolves handle"), i);
    }

    std::vector<uint8> has_los;
    has_los.resize(static_cast<std::size_t>(ends.num()));
    harness.get_simulation().get_spatial_query_manager().has_line_of_sight_to_targets(
        ml::make_vector3f(0.f, 0.f, 0.f), ends.get_const_view(), targets, has_los);
    for (int32 i{}; i < static_cast<int32>(has_los.size()); ++i) {
        checks.are_equal(uint8{1}, has_los[i], TEXT("Clear or target hit has line of sight"), i);
    }
    for (int32 i{}; i < count; ++i) {
        auto const other{expected[(i + 1) % count]};
        targets[i + count] = other;
        targets[i + 2 * count] = other;
    }
    harness.get_simulation().get_spatial_query_manager().has_line_of_sight_to_targets(
        ml::make_vector3f(0.f, 0.f, 0.f), ends.get_const_view(), targets, has_los);
    for (int32 i{}; i < count; ++i) {
        checks.are_equal(uint8{1}, has_los[i], TEXT("Clear line remains visible"), i);
        checks.are_equal(uint8{0}, has_los[i + count], TEXT("Other target is blocked"), i);
        checks.are_equal(
            uint8{0}, has_los[i + 2 * count], TEXT("Other target past hit is blocked"), i);
    }
    test.TestTrue(TEXT("Line-of-sight query batch completed"), true);
}

}
