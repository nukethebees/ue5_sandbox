#include <Sandbox/batch_game/SpatialQueryManager.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestCapitalShipFighters.h>
#include <Sandbox/batch_game/TestCapitalShipProxy.h>
#include <Sandbox/batch_game/TestCapitalShips.h>
#include <Sandbox/batch_game/TestEntity.h>
#include <Sandbox/batch_game/TestEntityType.h>

#include <SandboxCoreEngine/actor_utils.h>

#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include "test_spatial_query_line_of_sight_scenario.h"
#include "test_spatial_query_resolution_scenario.h"

#include <Components/InstancedStaticMeshComponent.h>
#include <Misc/Optional.h>

#include <functional>

namespace ml {
FSpatialQueryResolutionScenario::FSpatialQueryResolutionScenario(FSimulationTestContext& context)
    : FSimulationTestScenario{context} {
    TestCommandBuilder.Do([this] { spawn_fixture(); });
}

void FSpatialQueryResolutionScenario::spawn_fixture() {
    auto* const first{spawn_capital_proxy(context_.world,
                                          context_.config,
                                          checks,
                                          TEXT("first_capital"),
                                          FVector{-4000.f, 0.f, 0.f})};
    auto* const second{spawn_capital_proxy(context_.world,
                                           context_.config,
                                           checks,
                                           TEXT("second_capital"),
                                           FVector{4000.f, 0.f, 0.f})};
    if (!checks.is_valid(first, TEXT("First capital is spawned")) ||
        !checks.is_valid(second, TEXT("Second capital is spawned"))) {
        return;
    }
    first->set_team(ETestTeam::Green);
    first->set_target_ship(second);
    second->set_team(ETestTeam::Red);
    second->set_target_ship(first);
}

auto FSpatialQueryResolutionScenario::make_hit(UPrimitiveComponent const& component,
                                               int32 const item) -> FSpatialQueryHit {
    return {&component, item};
}

void FSpatialQueryResolutionScenario::sort_hits_by_component(TArray<FSpatialQueryHit>& hits) {
    std::less<void const*> const pointer_less{};
    hits.Sort([pointer_less](FSpatialQueryHit const& lhs, FSpatialQueryHit const& rhs) {
        return pointer_less(lhs.component, rhs.component);
    });
}

auto FSpatialQueryResolutionScenario::get_expected_type(
    UPrimitiveComponent const* const component) const -> ETestEntityType {
    check(component == capital_instances || component == fighter_instances);
    return component == capital_instances ? ETestEntityType::CapitalShip
                                          : ETestEntityType::CapitalShipFighter;
}

void FSpatialQueryResolutionScenario::initial_setup() {
    initialise_test_driver();
    auto& orchestrator{test_driver->orchestrator};
    orchestrator.start_simulation();
    capitals = const_cast<ATestCapitalShips*>(orchestrator.get_capital_ships());
    fighters = const_cast<ATestCapitalShipFighters*>(orchestrator.get_capital_ship_fighters());
    if (capitals) {
        capital_instances = capitals->FindComponentByClass<UInstancedStaticMeshComponent>();
    }
    if (fighters) {
        fighter_instances = fighters->FindComponentByClass<UInstancedStaticMeshComponent>();
    }
    orchestrator.set_end_tick_test_hook(FOrchestratorEndTickTestHook::CreateRaw(
        this, &FSpatialQueryResolutionScenario::on_end_tick));
    test_driver->timeline.at(query_time, [this] { resolve_hits(); }).finish_at(query_time);
}

void FSpatialQueryResolutionScenario::resolve_hits() {
    checks.is_valid(capitals, TEXT("Capital ship actor is available"));
    checks.is_valid(fighters, TEXT("Capital ship fighter actor is available"));
    checks.not_nullptr(capital_instances, TEXT("Capital ship collision component is available"));
    checks.not_nullptr(fighter_instances,
                       TEXT("Capital ship fighter collision component is available"));
    if (!capitals || !fighters || !capital_instances || !fighter_instances) {
        return;
    }

    checks.is_greater_than(
        capitals->get_num_instances(), int32{1}, TEXT("At least two capital ship instances"));
    checks.is_greater_than(
        fighters->get_num_instances(), int32{1}, TEXT("At least two fighter instances"));
    if (capitals->get_num_instances() < 2 || fighters->get_num_instances() < 2) {
        return;
    }

    TArray<FSpatialQueryHit> hits{make_hit(*capital_instances, 0),
                                  make_hit(*capital_instances, 1),
                                  make_hit(*fighter_instances, 1),
                                  make_hit(*fighter_instances, 0)};
    sort_hits_by_component(hits);
    TArray<FRegistryEntityHandle> handles;
    handles.SetNumUninitialized(hits.Num());
    test_driver->orchestrator.get_spatial_query_manager().resolve_hits(hits, handles);
    checks.are_equal(hits.Num(), handles.Num(), TEXT("One handle is returned for every hit"));
    auto const n_hits{hits.Num()};
    for (int32 i{0}; i < n_hits; ++i) {
        auto const handle{handles[i]};
        checks.is_true(!handle.is_null(), TEXT("Resolved handle is non-null"), i);
        if (!handle.is_null()) {
            checks.are_equal(get_expected_type(hits[i].component),
                             test_driver->orchestrator.get_entity_type(handle),
                             TEXT("Resolved handle has the expected entity type"),
                             i);
        }
    }
    queried_hits = true;
}

void FSpatialQueryResolutionScenario::on_end_tick(ATestBatchOrchestrator&) {
    test_driver->advance_timeline();
}

void FSpatialQueryResolutionScenario::run_checks() {
    checks.is_true(queried_hits, TEXT("Hit resolution ran at the requested simulation time"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}

void FSpatialQueryResolutionScenario::run() {
    run_until_timeline_finished([this] { initial_setup(); }, timeout, [this] { run_checks(); });
}

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
