#include "test_spatial_query_empty_scenario.h"

#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/test_setup.h>

#include <SpaceGame/entities/ProxyEntityMap.h>
#include <SpaceGame/entities/TestEntity.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/simulation/SpatialQueryManager.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

#include <SandboxCoreEngine/actor_utils.h>
#include <SandboxTests/support/TestActorSpawning.h>

namespace ml {
FSpatialQueryEmptyScenario::FSpatialQueryEmptyScenario(FSimulationTestContext& context)
    : FSimulationTestScenario{context} {}

void FSpatialQueryEmptyScenario::on_tear_down() {
    if (driver.IsSet()) {
        driver->orchestrator.clear_end_tick_test_hook();
    }
}

/* ------------------------------------------------------------------------------------------ */
// Empty batch and empty-world queries
/* ------------------------------------------------------------------------------------------ */
void FSpatialQueryEmptyScenario::run_queries() {
    auto& queries{driver->orchestrator.get_spatial_query_manager()};
    TArray<FRegistryEntityHandle> handles;

    FVectors3f starts;
    FVectors3f ends;
    queries.trace_line_of_sight(starts.get_const_view(), ends.get_const_view(), handles);

    TArray<uint8> line_of_sight;
    queries.has_line_of_sight_to_targets(
        FVector3f::ZeroVector, ends.get_const_view(), handles, line_of_sight);

    TArray<uint8> clear_lines;
    queries.have_clear_lines(starts.get_const_view(), ends.get_const_view(), clear_lines);

    current_sample.entities_in_range = queries.collect_non_team_entities_in_range(
        FVector3f::ZeroVector, ETestTeam::Blue, 1000.f, handles);
    current_sample.any_entity_is_null = queries.get_any_non_team_entity(ETestTeam::Blue).is_null();
    current_sample.queries_completed = true;
}

void FSpatialQueryEmptyScenario::on_end_tick(ATestBatchOrchestrator&) {
    samples.add(driver->get_time(), current_sample);
    driver->timeline.tick(driver->get_time());
}

void FSpatialQueryEmptyScenario::check_results() {
    checks.is_true(!samples.is_empty(), TEXT("Empty-query results are sampled"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    auto const& sample{samples.last_value()};
    checks.is_true(sample.queries_completed, TEXT("All empty query batches complete"));
    checks.are_equal(0, sample.entities_in_range, TEXT("Empty world has no entities in range"));
    checks.is_true(sample.any_entity_is_null, TEXT("Empty world has no arbitrary enemy"));
}

void FSpatialQueryEmptyScenario::run() {
    TestCommandBuilder
        .Do([this] {
            driver = TestSimulationDriver::from_world(context_.world);
            driver->orchestrator.set_end_tick_test_hook(FOrchestratorEndTickTestHook::CreateRaw(
                this, &FSpatialQueryEmptyScenario::on_end_tick));
            driver->timeline.at(0.1, [this] { run_queries(); }).finish_at(0.2);
            driver->orchestrator.start_simulation();
        })
        .Until([this] { return driver->timeline.is_finished(); }, FTimespan{0, 0, 2})
        .Then([this] {
            check_results();
            SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
        });
}

FSpatialQueryRangeScenario::FSpatialQueryRangeScenario(FSimulationTestContext& context)
    : FSimulationTestScenario{context} {
    TestCommandBuilder.Do([this] {
        static TStaticArray<FName, 4> const names{
            TEXT("origin"), TEXT("friendly"), TEXT("boundary_enemy"), TEXT("outside_enemy")};
        static TStaticArray<FVector, 4> const locations{FVector::ZeroVector,
                                                        FVector{500.f, 0.f, 0.f},
                                                        FVector{1000.f, 0.f, 0.f},
                                                        FVector{1000.1f, 0.f, 0.f}};
        spawn_actors<ATestCapitalShipProxy, 4>(
            context_.world,
            [this](ATestCapitalShipProxy& actor, int32 const i, ESpawnPhase const phase) {
                if (phase == ESpawnPhase::PreSpawn) {
                    actor.set_test_name(names[i]);
                    actor.set_team(i < 2 ? ETestTeam::Blue : ETestTeam::Red);
                    actor.set_initial_spawn_delay(60.f);
                    return;
                }
                actor.SetActorLocation(locations[i]);
            });
        ATestBatchOrchestrator::on_proxy_entities_bound.AddRaw(this,
                                                               &FSpatialQueryRangeScenario::bind);
    });
}

void FSpatialQueryRangeScenario::on_tear_down() {
    ATestBatchOrchestrator::on_proxy_entities_bound.RemoveAll(this);
    if (driver.IsSet()) {
        driver->orchestrator.clear_end_tick_test_hook();
    }
}

/* ------------------------------------------------------------------------------------------ */
// Team and inclusive-radius filtering
/* ------------------------------------------------------------------------------------------ */
void FSpatialQueryRangeScenario::bind(FProxyEntityMap const& proxies) {
    for (auto const& [actor, identifiers] : proxies) {
        auto const* const entity{Cast<ITestEntity>(actor)};
        if (entity && entity->get_test_name() == TEXT("boundary_enemy")) {
            boundary_enemy = identifiers.handle;
        }
    }
    ATestBatchOrchestrator::on_proxy_entities_bound.RemoveAll(this);
}

void FSpatialQueryRangeScenario::run_query() {
    TStaticArray<FRegistryEntityHandle, 4> results;
    auto const count{
        driver->orchestrator.get_spatial_query_manager().collect_non_team_entities_in_range(
            FVector3f::ZeroVector, ETestTeam::Blue, 1000.f, results)};
    query_result.Reset(count);
    query_result.Append(results.GetData(), count);
}

void FSpatialQueryRangeScenario::on_end_tick(ATestBatchOrchestrator&) {
    samples.add(driver->get_time(), query_result);
    driver->timeline.tick(driver->get_time());
}

void FSpatialQueryRangeScenario::check_results() {
    checks.is_true(boundary_enemy.is_valid(), TEXT("Boundary enemy is bound"));
    checks.is_true(!samples.is_empty(), TEXT("Range-query results are sampled"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    auto const& result{samples.last_value()};
    checks.are_equal(1, result.Num(), TEXT("Only one non-team entity is within range"));
    if (result.Num() == 1) {
        checks.are_equal(boundary_enemy, result[0], TEXT("Entity exactly on radius is included"));
    }
}

void FSpatialQueryRangeScenario::run() {
    TestCommandBuilder
        .Do([this] {
            driver = TestSimulationDriver::from_world(context_.world);
            driver->orchestrator.set_end_tick_test_hook(FOrchestratorEndTickTestHook::CreateRaw(
                this, &FSpatialQueryRangeScenario::on_end_tick));
            driver->timeline.at(0.1, [this] { run_query(); }).finish_at(0.2);
            driver->orchestrator.start_simulation();
        })
        .Until([this] { return driver->timeline.is_finished(); }, FTimespan{0, 0, 2})
        .Then([this] {
            check_results();
            SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
        });
}
}
