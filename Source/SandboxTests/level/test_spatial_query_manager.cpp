#include <SpaceGame/entities/TestEntity.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/simulation/SpatialQueryManager.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

#include <SandboxCoreEngine/actor_utils.h>

#include <Misc/Optional.h>
#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include "test_spatial_query_line_of_sight_scenario.h"

namespace ml {
FSpatialQueryLineOfSightScenario::FSpatialQueryLineOfSightScenario(FSimulationTestContext& context)
    : FSimulationTestScenario{context} {
    expected.Init({}, names.Num());
    TestCommandBuilder.Do([this] {
        auto& world{context_.world};
        ml::spawn_actors<ATestCapitalShipProxy, 4>(
            world, [this](ATestCapitalShipProxy& actor, int32 const i, ESpawnPhase const phase) {
                if (phase == ESpawnPhase::PreSpawn) {
                    actor.set_test_name(names[i]);
                    actor.set_initial_spawn_delay(spawn_cooldown);
                    actor.set_spawn_cooldown(spawn_cooldown);
                    return;
                }

                actor.SetActorLocation(FVector{locations[i]});
            });

        ATestBatchOrchestrator::on_proxy_entities_bound.AddRaw(
            this, &FSpatialQueryLineOfSightScenario::bind);
    });
}

void FSpatialQueryLineOfSightScenario::on_tear_down() {
    ATestBatchOrchestrator::on_proxy_entities_bound.RemoveAll(this);
}

void FSpatialQueryLineOfSightScenario::bind(FProxyEntityMap const& proxies) {
    check(proxies.Num() == names.Num());
    for (auto const& [actor, identifiers] : proxies) {
        auto const* const entity{Cast<ITestEntity>(actor)};
        check(entity);

        auto const i{names.Find(entity->get_test_name())};
        check(i != INDEX_NONE);

        expected[i] = entity->get_entity_handle();
        check(!expected[i].is_null());
    }
    ATestBatchOrchestrator::on_proxy_entities_bound.RemoveAll(this);
}

void FSpatialQueryLineOfSightScenario::run_queries() {
    FVectors3f starts;
    FVectors3f ends;
    TArray<FRegistryEntityHandle> targets;

    TStaticArray<float, 3> scales{0.5f, 1.f, 2.f};
    auto const n_scales{scales.Num()};

    starts.reserve(n_scales * locations.Num());
    ends.reserve(n_scales * locations.Num());
    targets.Reserve(n_scales * locations.Num());

    for (float const scale : scales) {
        auto const n_locations{locations.Num()};
        for (int32 i{}; i < n_locations; ++i) {
            starts.add(FVector3f::ZeroVector);
            ends.add(locations[i] * scale);
            targets.Add(expected[i]);
        }
    }

    TArray<FRegistryEntityHandle> results;
    results.SetNumUninitialized(ends.num());

    test_driver->orchestrator.get_spatial_query_manager().trace_line_of_sight(
        starts.get_const_view(), ends.get_const_view(), results);

    auto const n_names{names.Num()};
    FRegistryEntityHandle const null_handle{};

    for (int32 i{}; i < n_names; ++i) {
        checks.are_equal(null_handle, results[i], TEXT("Half-distance trace misses"), i);
    }

    for (int32 i{}; i < n_names; ++i) {
        checks.are_equal(expected[i], results[i + n_names], TEXT("Ship trace resolves handle"), i);
    }

    for (int32 i{}; i < n_names; ++i) {
        checks.are_equal(
            expected[i], results[i + 2 * n_names], TEXT("Past-ship trace resolves handle"), i);
    }

    TArray<uint8> has_los;
    has_los.SetNumUninitialized(ends.num());
    test_driver->orchestrator.get_spatial_query_manager().has_line_of_sight_to_targets(
        FVector3f::ZeroVector, ends.get_const_view(), targets, has_los);

    auto const n_endpoints{ends.num()};
    for (int32 i{}; i < n_endpoints; ++i) {
        checks.are_equal(uint8{1}, has_los[i], TEXT("Clear or target hit has line of sight"), i);
    }

    for (int32 i{}; i < n_names; ++i) {
        auto const other_target_index{(i + 1) % n_names};
        targets[i + n_names] = expected[other_target_index];
        targets[i + 2 * n_names] = expected[other_target_index];
    }

    test_driver->orchestrator.get_spatial_query_manager().has_line_of_sight_to_targets(
        FVector3f::ZeroVector, ends.get_const_view(), targets, has_los);

    for (int32 i{}; i < n_names; ++i) {
        checks.are_equal(uint8{1}, has_los[i], TEXT("Clear line remains visible"), i);
        checks.are_equal(
            uint8{0}, has_los[i + n_names], TEXT("Other target is blocked by first hit"), i);
        checks.are_equal(
            uint8{0}, has_los[i + 2 * n_names], TEXT("Other target past first hit is blocked"), i);
    }

    TArray<uint8> clear_lines;
    clear_lines.SetNumUninitialized(ends.num());
    test_driver->orchestrator.get_spatial_query_manager().have_clear_lines(
        starts.get_const_view(), ends.get_const_view(), clear_lines);

    for (int32 i{}; i < n_names; ++i) {
        checks.are_equal(uint8{1}, clear_lines[i], TEXT("Half-distance line is clear"), i);
        checks.are_equal(
            uint8{0}, clear_lines[i + n_names], TEXT("Line ending at ship is blocked"), i);
        checks.are_equal(
            uint8{0}, clear_lines[i + 2 * n_names], TEXT("Line passing ship is blocked"), i);
    }

    TArray<FRegistryEntityHandle> ignored_entities;
    ignored_entities.Reserve(ends.num());
    for (int32 scale_index{}; scale_index < n_scales; ++scale_index) {
        ignored_entities.Append(expected);
    }
    test_driver->orchestrator.get_spatial_query_manager().have_clear_lines(
        starts.get_const_view(), ends.get_const_view(), clear_lines, ignored_entities);
    for (int32 i{}; i < n_endpoints; ++i) {
        checks.are_equal(uint8{1}, clear_lines[i], TEXT("Ignored entity does not block line"), i);
    }

    auto const& queries{test_driver->orchestrator.get_spatial_query_manager()};
    auto const scalar_hit{queries.trace_closest(FVector3f::ZeroVector, locations[0] * 2.f)};
    checks.is_true(scalar_hit.hit, TEXT("Scalar closest trace reports a hit"));
    checks.are_equal(expected[0], scalar_hit.entity, TEXT("Scalar closest trace resolves handle"));
    checks.is_true(queries.has_clear_line(FVector3f::ZeroVector, locations[0] * 0.5f),
                   TEXT("Scalar half-distance line is clear"));
    checks.is_true(!queries.has_clear_line(FVector3f::ZeroVector, locations[0] * 2.f),
                   TEXT("Scalar line passing ship is blocked"));
    checks.is_true(queries.has_clear_line(FVector3f::ZeroVector, locations[0] * 2.f, expected[0]),
                   TEXT("Scalar ignored entity does not block line"));
}

void FSpatialQueryLineOfSightScenario::on_end_tick(ATestBatchOrchestrator&) {
    test_driver->advance_timeline();
}

void FSpatialQueryLineOfSightScenario::run() {
    run_until_timeline_finished(
        [this] {
            initialise_test_driver();
            test_driver->orchestrator.set_end_tick_test_hook(
                FOrchestratorEndTickTestHook::CreateRaw(
                    this, &FSpatialQueryLineOfSightScenario::on_end_tick));
            test_driver->timeline.at(0.1, [this] { run_queries(); }).finish_at(0.2);
            test_driver->orchestrator.start_simulation();
        },
        FTimespan{0, 0, 2},
        [this] { SANDBOX_TESTS_ASSERT_ALL_PASSED(checks); });
}
}
