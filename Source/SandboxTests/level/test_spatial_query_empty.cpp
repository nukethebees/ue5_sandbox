#include "test_spatial_query_empty.h"
#include <array>
#include <SpaceGameSimulation/simulation/NativeVectorTypes.h>
#include <vector>

#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/test_setup.h>

#include <sandbox/simulation/simulation/SpatialQueryManager.h>
#include <SpaceGame/entities/ProxyEntityMap.h>
#include <SpaceGame/entities/TestEntity.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

#include <SandboxCoreEngine/actor_utils.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/WorldlessSimulationTest.h>

namespace ml {
void run_worldless_spatial_query_empty(FAutomationTestBase& test,
                                       FSoftTestAssertions& checks,
                                       USpaceGameLevelConfig const& config) {
    auto data{make_worldless_simulation_test_data(config)};
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_ships.fighter_spawn_slots_relative_transforms.clear();
    FWorldlessSimulationTest harness{MoveTemp(data)};
    harness.finish_initialisation();
    auto& queries{harness.get_simulation().get_spatial_query_manager()};
    std::vector<FRegistryEntityHandle> handles;
    ml::simulation::Vectors3f starts;
    ml::simulation::Vectors3f ends;
    queries.trace_line_of_sight(starts.get_const_view(), ends.get_const_view(), handles);
    std::vector<uint8> line_of_sight;
    queries.has_line_of_sight_to_targets(
        ml::make_vector3f(0.f, 0.f, 0.f), ends.get_const_view(), handles, line_of_sight);
    std::vector<FRegistryEntityHandle> results;
    auto const count{queries.collect_non_team_entities_in_range(
        ml::make_vector3f(0.f, 0.f, 0.f), ml::simulation::Team::Blue, 1000.f, results)};
    test.TestEqual(TEXT("Empty range query has no results"), count, 0);
    checks.is_true(queries.get_any_non_team_entity(ml::simulation::Team::Blue).is_null(),
                   TEXT("Empty world has no arbitrary enemy"));
}

void run_worldless_spatial_query_range(FAutomationTestBase& test,
                                       FSoftTestAssertions& checks,
                                       USpaceGameLevelConfig const& config) {
    auto data{make_worldless_simulation_test_data(config)};
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_ships.fighter_spawn_slots_relative_transforms.clear();
    add_worldless_capital_spawn(data, FVector3f::ZeroVector, ETestTeam::Blue);
    add_worldless_capital_spawn(data, FVector3f{500.f, 0.f, 0.f}, ETestTeam::Blue);
    add_worldless_capital_spawn(data, FVector3f{1000.f, 0.f, 0.f}, ETestTeam::Red);
    add_worldless_capital_spawn(data, FVector3f{1000.1f, 0.f, 0.f}, ETestTeam::Red);
    FWorldlessSimulationTest harness{MoveTemp(data)};
    harness.finish_initialisation();
    auto const ignored_origin{harness.get_simulation().get_capital_ships().get_handle(0)};
    auto const friendly{harness.get_simulation().get_capital_ships().get_handle(1)};
    auto const boundary_enemy{harness.get_simulation().get_capital_ships().get_handle(2)};
    std::array<FRegistryEntityHandle, 4> results;
    auto const count{
        harness.get_simulation().get_spatial_query_manager().collect_non_team_entities_in_range(
            ml::make_vector3f(0.f, 0.f, 0.f), ml::simulation::Team::Blue, 1000.f, results)};
    test.TestEqual(TEXT("Only one enemy is within the inclusive radius"), count, 1);
    if (count == 1) {
        checks.are_equal(boundary_enemy, results[0], TEXT("Boundary enemy is included"));
    }

    auto const type_count{
        harness.get_simulation().get_spatial_query_manager().collect_entities_of_type_in_range(
            ml::make_vector3f(0.f, 0.f, 0.f),
            ml::simulation::EntityType::CapitalShip,
            1000.f,
            ignored_origin,
            results)};
    test.TestEqual(
        TEXT("Type-filtered query ignores self and includes two capitals"), type_count, 2);
    if (type_count == 2) {
        checks.are_equal(friendly, results[0], TEXT("Type-filtered order is deterministic"));
        checks.are_equal(
            boundary_enemy, results[1], TEXT("Type-filtered query includes the boundary entity"));
    }
}

}
