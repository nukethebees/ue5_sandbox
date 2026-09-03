#include "test_collision_uniform_grid_scenario.h"

#include <SandboxTests/support/SimulationTestAssets.h>
#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestActorSpawning.h>

#include <SpaceGame/defences/spinners/TestTubeSpinnerProxy.h>
#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/entities/TestEntity.h>
#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/entities/TestEntityType.h>
#include <SpaceGame/entities/TestTeam.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/capital/TestCapitalShips.h>
#include <SpaceGame/ships/fighters/TestCapitalShipFighters.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/collision_uniform_grid.h>
#include <SpaceGame/simulation/CollisionSystem.h>
#include <SpaceGame/simulation/LineTraces.h>
#include <SpaceGame/simulation/SpatialQueryManager.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGame/simulation/TraceHits.h>

#include <SandboxCoreEngine/actor_utils.h>

namespace ml {
namespace {
FIntVector3 const grid_dims{400, 400, 5};
FVector3f const cell_dims{5000.f, 5000.f, 20000.f};
FIntVector3 const trace_grid_dims{8, 8, 8};
FVector3f const trace_cell_dims{100.f, 100.f, 100.f};
constexpr float hit_location_tolerance{0.001f};

struct FTraceFixture {
    FTraceFixture(TConstArrayView<FVector3f> const locations,
                  FVector3f const half_extents,
                  FVector3f const aabb_centre = FVector3f::ZeroVector) {
        FVectors3f registry_locations;
        FVectors3f velocities;
        auto const count{locations.Num()};
        registry_locations.reserve(count);
        velocities.reserve(count);

        for (auto const location : locations) {
            registry_locations.add(location);
            velocities.add(FVector3f::ZeroVector);
        }

        TArray<float> radii;
        TArray<int32> healths;
        TArray<ETestTeam> teams;
        TArray<ETestEntityType> entity_types;
        TArray<uint8> alive;
        radii.Init(0.f, count);
        healths.Init(1, count);
        teams.Init(ETestTeam::Blue, count);
        entity_types.Init(ETestEntityType::CapitalShip, count);
        alive.Init(uint8{1}, count);

        FTestEntityRegistry::EntityData::ConstView const entity_data{
            .locations = registry_locations.get_const_view(),
            .velocities = velocities.get_const_view(),
            .radii = radii,
            .healths = healths,
            .teams = teams,
            .entity_types = entity_types,
            .alive = alive,
        };
        auto const spawned{registry.add_entities(entity_data)};
        handles = spawned.registry_handles.to_array();

        auto constexpr aabb_index{ioj::FEntityAABBs::capital_ship_index};
        ioj::FEntityAABBs aabbs;
        aabbs.centre_xs[aabb_index] = aabb_centre.X;
        aabbs.centre_ys[aabb_index] = aabb_centre.Y;
        aabbs.centre_zs[aabb_index] = aabb_centre.Z;
        aabbs.half_extent_xs[aabb_index] = half_extents.X;
        aabbs.half_extent_ys[aabb_index] = half_extents.Y;
        aabbs.half_extent_zs[aabb_index] = half_extents.Z;

        grid.set_grid_dims(trace_grid_dims);
        grid.set_cell_dims(trace_cell_dims);
        grid.set_entity_registry(registry);
        grid.rebuild_grid(aabbs);
    }

    FTestEntityRegistry registry;
    ioj::CollisionUniformGrid grid;
    TArray<FRegistryEntityHandle> handles;
};

auto run_traces(FTraceFixture const& fixture,
                TConstArrayView<FVector3f> const starts,
                TConstArrayView<FVector3f> const ends) -> FTraceHits {
    check(starts.Num() == ends.Num());

    FLineTraces traces;
    auto const count{starts.Num()};
    traces.starts.reserve(count);
    traces.ends.reserve(count);
    for (int32 i{}; i < count; ++i) {
        traces.starts.add(starts[i]);
        traces.ends.add(ends[i]);
    }

    FTraceHits hits;
    hits.add_defaulted(count);
    fixture.grid.trace_aabbs(traces.get_const_view(), hits.get_view());
    return hits;
}

struct FExpectedTrace {
    TCHAR const* name;
    FVector3f start;
    FVector3f end;
    uint8 expected_hit;
    FVector3f expected_location{};
    int32 expected_entity_index{};
};

void check_traces(FSoftTestAssertions& checks,
                  FTraceFixture const& fixture,
                  TConstArrayView<FExpectedTrace> const cases) {
    TArray<FVector3f> starts;
    TArray<FVector3f> ends;
    starts.Reserve(cases.Num());
    ends.Reserve(cases.Num());

    for (auto const& trace_case : cases) {
        starts.Add(trace_case.start);
        ends.Add(trace_case.end);
    }

    auto const hits{run_traces(fixture, starts, ends)};
    auto const count{cases.Num()};
    for (int32 i{}; i < count; ++i) {
        auto const& trace_case{cases[i]};
        FString const hit_description{
            FString::Printf(TEXT("%s has expected hit flag"), trace_case.name)};
        checks.are_equal(trace_case.expected_hit, hits.hits[i], hit_description);
        if (trace_case.expected_hit == 0 || hits.hits[i] == 0) {
            continue;
        }

        FString const entity_description{
            FString::Printf(TEXT("%s resolves expected entity"), trace_case.name)};
        checks.are_equal(fixture.handles[trace_case.expected_entity_index],
                         hits.entities[i],
                         entity_description);

        FString const location_description{
            FString::Printf(TEXT("%s resolves expected hit location"), trace_case.name)};
        checks.dist_zero(trace_case.expected_location,
                         hits.locations[i],
                         hit_location_tolerance,
                         location_description);
    }
}

auto count_handle(TConstArrayView<FRegistryEntityHandle> const handles,
                  FRegistryEntityHandle const expected) -> int32 {
    int32 count{};
    for (auto const handle : handles) {
        if (handle == expected) {
            ++count;
        }
    }
    return count;
}
}

FCollisionUniformGridScenario::FCollisionUniformGridScenario(FSimulationTestContext& context)
    : FSimulationTestScenario{context} {
    TestCommandBuilder.Do([this] { spawn_fixture(); });
}

void FCollisionUniformGridScenario::on_tear_down() {
    ATestBatchOrchestrator::on_proxy_entities_bound.RemoveAll(this);
}

void FCollisionUniformGridScenario::spawn_fixture() {
    FTransform const player_transform{FRotator::ZeroRotator, FVector{-1500.f, -1500.f, 0.f}};
    auto* const player{spawn_player_ship(context_.world,
                                         context_.config.classes.player_ship_class,
                                         &context_.config.player_ship,
                                         player_transform)};
    if (!checks.is_valid(player, TEXT("Collision-grid player ship is spawned"))) {
        return;
    }
    context_.orchestrator.set_player_ship(*player);

    auto* const capital{spawn_capital_proxy(context_.world,
                                            context_.config,
                                            checks,
                                            TEXT("collision_grid_capital"),
                                            FVector{-500.f, -500.f, 0.f})};
    if (!checks.is_valid(capital, TEXT("Collision-grid capital ship is spawned"))) {
        return;
    }

    auto* const capital_config{duplicate_capital_ships_config(context_.config, *capital)};
    if (!checks.not_nullptr(capital_config, TEXT("Collision-grid capital config is created"))) {
        return;
    }
    capital_config->spawn_delay = 0.0f;
    capital->set_actor_config(capital_config);
    capital->set_team(ETestTeam::Blue);
    capital->set_target_ship(player);

    spawn_actors<ATestStaticTurretsProxy, 1>(
        context_.world, [this](ATestStaticTurretsProxy& actor, int32, ESpawnPhase const phase) {
            if (phase == ESpawnPhase::PreSpawn) {
                actor.set_actor_config(&context_.config.turrets);
                actor.set_test_name(TEXT("collision_grid_turret"));
                actor.set_team(ETestTeam::Blue);
                return;
            }
            actor.SetActorLocation(FVector{500.f, 500.f, 0.f});
        });

    spawn_actors<ATestTubeSpinnerProxy, 1>(
        context_.world, [this](ATestTubeSpinnerProxy& actor, int32, ESpawnPhase const phase) {
            if (phase == ESpawnPhase::PreSpawn) {
                actor.set_actor_config(&context_.config.tube_spinners);
                actor.set_test_name(TEXT("collision_grid_spinner"));
                return;
            }
            actor.SetActorLocation(FVector{1500.f, 1500.f, 0.f});
        });

    ATestBatchOrchestrator::on_proxy_entities_bound.AddRaw(
        this, &FCollisionUniformGridScenario::bind_proxy_entities);
}

void FCollisionUniformGridScenario::bind_proxy_entities(FProxyEntityMap const& proxies) {
    for (auto const& [actor, identifiers] : proxies) {
        auto const* const entity{Cast<ITestEntity>(actor)};
        if (entity == nullptr) {
            continue;
        }

        auto const name{entity->get_test_name()};
        if (name == TEXT("collision_grid_capital")) {
            expected_handles_[std::to_underlying(ETestEntityType::CapitalShip)] =
                identifiers.handle;
        } else if (name == TEXT("collision_grid_turret")) {
            expected_handles_[std::to_underlying(ETestEntityType::Turret)] = identifiers.handle;
        } else if (name == TEXT("collision_grid_spinner")) {
            expected_handles_[std::to_underlying(ETestEntityType::TubeSpinner)] =
                identifiers.handle;
        }
    }
}

void FCollisionUniformGridScenario::initialise_simulation() {
    initialise_test_driver();

    test_driver->orchestrator.start_simulation();

    auto const* const player{test_driver->orchestrator.get_player_ship()};
    auto const* const capitals{test_driver->orchestrator.get_capital_ships()};
    auto const* const fighters{test_driver->orchestrator.get_capital_ship_fighters()};
    if (!checks.is_valid(player, TEXT("Collision-grid player ship is available")) ||
        !checks.is_valid(capitals, TEXT("Collision-grid capital ships are available")) ||
        !checks.is_valid(fighters, TEXT("Collision-grid fighters are available"))) {
        return;
    }

    expected_handles_[std::to_underlying(ETestEntityType::PlayerShip)] =
        player->get_entity_handle();

    auto const& grid{test_driver->orchestrator.get_spatial_query_manager()
                         .get_collision_system()
                         .get_uniform_grid()};
    checks.is_true(grid.get_grid_dims() == grid_dims,
                   TEXT("Collision grid uses the production dimensions"));
    checks.is_true(grid.get_cell_dims() == cell_dims,
                   TEXT("Collision grid uses the production cell size"));

    test_driver->orchestrator.set_end_tick_test_hook(
        FOrchestratorEndTickTestHook::CreateRaw(this, &FCollisionUniformGridScenario::on_end_tick));
    test_driver->timeline.at(sample_time, [this] { sample_grid(); }).finish_at(sample_time);
}

void FCollisionUniformGridScenario::sample_grid() {
    auto& orchestrator{test_driver->orchestrator};
    auto const& registry{orchestrator.get_entity_registry()};
    auto& collision{orchestrator.get_spatial_query_manager().get_collision_system()};
    auto& grid{collision.get_uniform_grid()};
    auto const& entity_aabbs{collision.get_entity_aabbs()};

    auto const* const fighters{orchestrator.get_capital_ship_fighters()};
    checks.is_valid(fighters, TEXT("Collision-grid fighters are available at sample time"));
    checks.is_true(fighters && !fighters->get_handles().IsEmpty(),
                   TEXT("Collision-grid fighter is placed"));
    if (fighters && !fighters->get_handles().IsEmpty()) {
        expected_handles_[std::to_underlying(ETestEntityType::CapitalShipFighter)] =
            fighters->get_handles()[0];
    }

    FSample sample{};
    auto const handle_count{expected_handles_.Num()};
    sample.expected_cell_counts.Reserve(handle_count);
    sample.found_cell_counts.Reserve(handle_count);

    for (int32 i{}; i < handle_count; ++i) {
        auto const handle{expected_handles_[i]};
        auto const entity_type{static_cast<ETestEntityType>(i)};
        checks.is_true(registry.is_valid_alive(handle),
                       FString::Printf(TEXT("Expected collision-grid %s entity is alive"),
                                       LexToString(entity_type)));
        if (!registry.is_valid_alive(handle)) {
            sample.expected_cell_counts.Add(0);
            sample.found_cell_counts.Add(0);
            continue;
        }

        auto const registered_type{registry.get_entity_type(handle)};
        auto const location{registry.get_location(handle)};
        auto const half_extents{entity_aabbs.get_half_extents(std::to_underlying(registered_type))};
        auto const min_coord{grid.to_min_cell_coord(location - half_extents)};
        auto const max_coord{grid.to_max_cell_coord(location + half_extents)};

        auto const is_in_bounds{grid.is_cell_coord_in_bounds(min_coord) &&
                                grid.is_cell_coord_in_bounds(max_coord)};
        checks.is_true(is_in_bounds,
                       FString::Printf(TEXT("Expected collision-grid %s entity is placed"),
                                       LexToString(entity_type)));
        if (!is_in_bounds) {
            sample.expected_cell_counts.Add(0);
            sample.found_cell_counts.Add(0);
            continue;
        }

        int32 expected_cell_count{};
        int32 found_cell_count{};
        for (int32 x{min_coord.X}; x <= max_coord.X; ++x) {
            for (int32 y{min_coord.Y}; y <= max_coord.Y; ++y) {
                for (int32 z{min_coord.Z}; z <= max_coord.Z; ++z) {
                    ++expected_cell_count;
                    found_cell_count += count_handle(grid.get_cell_entities({x, y, z}), handle);
                }
            }
        }

        sample.expected_cell_counts.Add(expected_cell_count);
        sample.found_cell_counts.Add(found_cell_count);
    }

    samples_.add(test_driver->get_time(), MoveTemp(sample));
}

void FCollisionUniformGridScenario::on_end_tick(ATestBatchOrchestrator&) {
    test_driver->advance_timeline();
}

void FCollisionUniformGridScenario::check_results() {
    checks.is_true(!samples_.is_empty(), TEXT("Collision-grid membership sample is recorded"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    auto const& sample{samples_.last_value()};
    auto const handle_count{expected_handles_.Num()};
    checks.are_equal(handle_count,
                     sample.expected_cell_counts.Num(),
                     TEXT("All expected entity types have a cell count"));
    checks.are_equal(handle_count,
                     sample.found_cell_counts.Num(),
                     TEXT("All expected entity types have a membership count"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    for (int32 i{}; i < handle_count; ++i) {
        checks.are_equal(sample.expected_cell_counts[i],
                         sample.found_cell_counts[i],
                         TEXT("Expected entity has the expected collision-grid membership"),
                         i);
    }
}

void FCollisionUniformGridScenario::run() {
    run_until_timeline_finished(
        [this] { initialise_simulation(); }, timeout, [this] { check_results(); });
}

/* ------------------------------------------------------------------------------------------ */
// Trace scenarios
/* ------------------------------------------------------------------------------------------ */
FCollisionUniformGridTraceScenario::FCollisionUniformGridTraceScenario(
    FSimulationTestContext& context, ECollisionUniformGridTraceScenario const scenario)
    : FSimulationTestScenario{context}
    , scenario_{scenario} {}

void FCollisionUniformGridTraceScenario::test_hits_and_misses() {
    FVector3f const entity_centre{};
    FVector3f const aabb_half_extents{10.f, 10.f, 10.f};
    FVector3f const left_of_aabb{-150.f, 0.f, 0.f};
    FVector3f const right_of_aabb{150.f, 0.f, 0.f};
    FVector3f const negative_diagonal_start{-150.f, -150.f, -150.f};
    FVector3f const positive_diagonal_end{150.f, 150.f, 150.f};
    FVector3f const off_axis_start{-150.f, 25.f, 0.f};
    FVector3f const off_axis_end{150.f, 25.f, 0.f};
    FVector3f const left_face_contact{-10.f, 0.f, 0.f};
    FVector3f const right_face_contact{10.f, 0.f, 0.f};
    FVector3f const negative_corner_contact{-10.f, -10.f, -10.f};
    FVector3f const unused_miss_location{};

    TArray<FVector3f> const entity_locations{entity_centre};
    FTraceFixture const fixture{entity_locations, aabb_half_extents};

    TArray<FVector3f> const starts{
        left_of_aabb,
        right_of_aabb,
        negative_diagonal_start,
        off_axis_start,
        entity_centre,
    };
    TArray<FVector3f> const ends{
        right_of_aabb,
        left_of_aabb,
        positive_diagonal_end,
        off_axis_end,
        right_of_aabb,
    };
    auto const hits{run_traces(fixture, starts, ends)};

    TStaticArray<uint8, 5> const expected_hit_flags{1, 1, 1, 0, 1};
    TStaticArray<FVector3f, 5> const expected_locations{
        left_face_contact,
        right_face_contact,
        negative_corner_contact,
        unused_miss_location,
        entity_centre,
    };

    auto const count{expected_hit_flags.Num()};
    for (int32 i{}; i < count; ++i) {
        checks.are_equal(
            expected_hit_flags[i], hits.hits[i], TEXT("Trace has expected hit flag"), i);
        if (expected_hit_flags[i] == 0) {
            continue;
        }

        checks.are_equal(
            fixture.handles[0], hits.entities[i], TEXT("Trace resolves expected entity"), i);
        checks.dist_zero(expected_locations[i],
                         hits.locations[i],
                         hit_location_tolerance,
                         TEXT("Trace resolves expected hit location"),
                         i);
    }
}

void FCollisionUniformGridTraceScenario::test_stops_at_endpoint() {
    FVector3f const entity_centre{50.f, 0.f, 0.f};
    FVector3f const aabb_half_extents{10.f, 10.f, 10.f};
    FVector3f const trace_start{};
    FVector3f const trace_endpoint_before_aabb{20.f, 0.f, 0.f};

    TArray<FVector3f> const entity_locations{entity_centre};
    FTraceFixture const fixture{entity_locations, aabb_half_extents};
    TArray<FVector3f> const starts{trace_start};
    TArray<FVector3f> const ends{trace_endpoint_before_aabb};

    auto const hits{run_traces(fixture, starts, ends)};

    checks.are_equal(uint8{0}, hits.hits[0], TEXT("AABB beyond trace endpoint is not hit"));
}

void FCollisionUniformGridTraceScenario::test_returns_nearest_hit() {
    FVector3f const far_entity_centre{60.f, 0.f, 0.f};
    FVector3f const near_entity_centre{20.f, 0.f, 0.f};
    FVector3f const aabb_half_extents{5.f, 5.f, 5.f};
    FVector3f const trace_start{};
    FVector3f const trace_end{90.f, 0.f, 0.f};
    FVector3f const expected_near_contact{15.f, 0.f, 0.f};

    TArray<FVector3f> const entity_locations{
        far_entity_centre,
        near_entity_centre,
    };
    FTraceFixture const fixture{entity_locations, aabb_half_extents};
    TArray<FVector3f> const starts{trace_start};
    TArray<FVector3f> const ends{trace_end};

    auto const hits{run_traces(fixture, starts, ends)};

    checks.are_equal(uint8{1}, hits.hits[0], TEXT("Trace through two AABBs records a hit"));
    checks.are_equal(
        fixture.handles[1], hits.entities[0], TEXT("Trace returns nearest intersecting entity"));
    checks.dist_zero(expected_near_contact,
                     hits.locations[0],
                     hit_location_tolerance,
                     TEXT("Trace returns nearest intersection location"));
}

void FCollisionUniformGridTraceScenario::test_handles_zero_length_traces() {
    FVector3f const entity_centre{50.f, 0.f, 0.f};
    FVector3f const aabb_half_extents{10.f, 10.f, 10.f};
    FVector3f const point_outside_aabb{};

    TArray<FVector3f> const entity_locations{entity_centre};
    FTraceFixture const fixture{entity_locations, aabb_half_extents};
    TArray<FVector3f> const starts{
        entity_centre,
        point_outside_aabb,
    };

    auto const hits{run_traces(fixture, starts, starts)};

    checks.are_equal(uint8{1}, hits.hits[0], TEXT("Stationary point inside AABB records a hit"));
    checks.are_equal(
        fixture.handles[0], hits.entities[0], TEXT("Stationary point resolves containing entity"));
    checks.dist_zero(starts[0],
                     hits.locations[0],
                     hit_location_tolerance,
                     TEXT("Stationary point hit location is the trace point"));
    checks.are_equal(
        uint8{0}, hits.hits[1], TEXT("Stationary point outside AABB does not record a hit"));
}

void FCollisionUniformGridTraceScenario::test_includes_negative_endpoint_boundary() {
    FVector3f const entity_centre{-10.f, 0.f, 0.f};
    FVector3f const aabb_half_extents{10.f, 10.f, 10.f};
    FVector3f const trace_start{10.f, 0.f, 0.f};
    FVector3f const boundary_contact{};

    TArray<FVector3f> const entity_locations{entity_centre};
    FTraceFixture const fixture{entity_locations, aabb_half_extents};
    TArray<FVector3f> const starts{trace_start};
    TArray<FVector3f> const ends{boundary_contact};

    auto const hits{run_traces(fixture, starts, ends)};

    checks.are_equal(uint8{1}, hits.hits[0], TEXT("Trace includes AABB touched at endpoint"));
    if (hits.hits[0] == 0) {
        return;
    }

    checks.are_equal(
        fixture.handles[0], hits.entities[0], TEXT("Endpoint trace resolves touched entity"));
    checks.dist_zero(boundary_contact,
                     hits.locations[0],
                     hit_location_tolerance,
                     TEXT("Endpoint trace returns boundary contact location"));
}

void FCollisionUniformGridTraceScenario::test_applies_aabb_centre() {
    FVector3f const entity_location{};
    FVector3f const local_aabb_centre{40.f, 0.f, 0.f};
    FVector3f const aabb_half_extents{10.f, 10.f, 10.f};
    FVector3f const trace_start{20.f, 0.f, 0.f};
    FVector3f const trace_end{60.f, 0.f, 0.f};
    FVector3f const expected_contact{30.f, 0.f, 0.f};

    TArray<FVector3f> const entity_locations{entity_location};
    FTraceFixture const fixture{entity_locations, aabb_half_extents, local_aabb_centre};
    TArray<FVector3f> const starts{trace_start};
    TArray<FVector3f> const ends{trace_end};

    auto const hits{run_traces(fixture, starts, ends)};

    checks.are_equal(uint8{1}, hits.hits[0], TEXT("Trace hits locally centred AABB"));
    if (hits.hits[0] == 0) {
        return;
    }

    checks.are_equal(
        fixture.handles[0], hits.entities[0], TEXT("Trace resolves locally centred entity"));
    checks.dist_zero(expected_contact,
                     hits.locations[0],
                     hit_location_tolerance,
                     TEXT("Trace applies local AABB centre to hit location"));
}

void FCollisionUniformGridTraceScenario::test_axis_parallel_and_origin() {
    FVector3f const entity_centre{};
    FVector3f const aabb_half_extents{10.f, 10.f, 10.f};
    constexpr float trace_extent{150.f};
    constexpr float outside_slab{11.f};
    constexpr float near_parallel_offset{9.f};
    constexpr float near_parallel_delta{0.0001f};
    auto const near_parallel_entry_y{near_parallel_offset +
                                     near_parallel_delta *
                                         ((trace_extent - 10.f) / (2.f * trace_extent))};

    TArray<FVector3f> const entity_locations{entity_centre};
    FTraceFixture const fixture{entity_locations, aabb_half_extents};
    TArray<FExpectedTrace> const cases{
        {TEXT("Positive X trace through origin"),
         {-trace_extent, 0.f, 0.f},
         {trace_extent, 0.f, 0.f},
         1,
         {-10.f, 0.f, 0.f}},
        {TEXT("Negative X trace through origin"),
         {trace_extent, 0.f, 0.f},
         {-trace_extent, 0.f, 0.f},
         1,
         {10.f, 0.f, 0.f}},
        {TEXT("Positive Y trace through origin"),
         {0.f, -trace_extent, 0.f},
         {0.f, trace_extent, 0.f},
         1,
         {0.f, -10.f, 0.f}},
        {TEXT("Negative Y trace through origin"),
         {0.f, trace_extent, 0.f},
         {0.f, -trace_extent, 0.f},
         1,
         {0.f, 10.f, 0.f}},
        {TEXT("Positive Z trace through origin"),
         {0.f, 0.f, -trace_extent},
         {0.f, 0.f, trace_extent},
         1,
         {0.f, 0.f, -10.f}},
        {TEXT("Negative Z trace through origin"),
         {0.f, 0.f, trace_extent},
         {0.f, 0.f, -trace_extent},
         1,
         {0.f, 0.f, 10.f}},
        {TEXT("X-parallel trace outside Y slab"),
         {-trace_extent, outside_slab, 0.f},
         {trace_extent, outside_slab, 0.f},
         0},
        {TEXT("Y-parallel trace outside X slab"),
         {outside_slab, -trace_extent, 0.f},
         {outside_slab, trace_extent, 0.f},
         0},
        {TEXT("Z-parallel trace outside X slab"),
         {outside_slab, 0.f, -trace_extent},
         {outside_slab, 0.f, trace_extent},
         0},
        {TEXT("Signed-zero components preserve axis-parallel hit"),
         {-trace_extent, -0.0f, 0.f},
         {trace_extent, 0.0f, -0.0f},
         1,
         {-10.f, 0.f, 0.f}},
        {TEXT("Near-parallel trace inside slab"),
         {-trace_extent, near_parallel_offset, 0.f},
         {trace_extent, near_parallel_offset + near_parallel_delta, 0.f},
         1,
         {-10.f, near_parallel_entry_y, 0.f}},
        {TEXT("Near-parallel trace outside slab"),
         {-trace_extent, outside_slab, 0.f},
         {trace_extent, outside_slab + near_parallel_delta, 0.f},
         0},
    };

    check_traces(checks, fixture, cases);
}

void FCollisionUniformGridTraceScenario::test_surface_contacts() {
    FVector3f const entity_centre{};
    FVector3f const aabb_half_extents{10.f, 10.f, 10.f};
    constexpr float outside_face{20.f};
    constexpr float face{10.f};
    constexpr float just_outside_face{10.5f};

    TArray<FVector3f> const entity_locations{entity_centre};
    FTraceFixture const fixture{entity_locations, aabb_half_extents};
    TArray<FExpectedTrace> const cases{
        {TEXT("Segment ends on negative X face"),
         {-outside_face, 0.f, 0.f},
         {-face, 0.f, 0.f},
         1,
         {-face, 0.f, 0.f}},
        {TEXT("Segment ends on positive X face"),
         {outside_face, 0.f, 0.f},
         {face, 0.f, 0.f},
         1,
         {face, 0.f, 0.f}},
        {TEXT("Segment ends on negative Y face"),
         {0.f, -outside_face, 0.f},
         {0.f, -face, 0.f},
         1,
         {0.f, -face, 0.f}},
        {TEXT("Segment ends on positive Y face"),
         {0.f, outside_face, 0.f},
         {0.f, face, 0.f},
         1,
         {0.f, face, 0.f}},
        {TEXT("Segment ends on negative Z face"),
         {0.f, 0.f, -outside_face},
         {0.f, 0.f, -face},
         1,
         {0.f, 0.f, -face}},
        {TEXT("Segment ends on positive Z face"),
         {0.f, 0.f, outside_face},
         {0.f, 0.f, face},
         1,
         {0.f, 0.f, face}},
        {TEXT("Segment starts on face and points outward"),
         {face, 0.f, 0.f},
         {outside_face, 0.f, 0.f},
         1,
         {face, 0.f, 0.f}},
        {TEXT("Segment starts on face and points inward"),
         {-face, 0.f, 0.f},
         {0.f, 0.f, 0.f},
         1,
         {-face, 0.f, 0.f}},
        {TEXT("Segment lies on AABB face"),
         {-outside_face, face, 0.f},
         {outside_face, face, 0.f},
         1,
         {-face, face, 0.f}},
        {TEXT("Segment lies on AABB edge"),
         {-outside_face, face, face},
         {outside_face, face, face},
         1,
         {-face, face, face}},
        {TEXT("Segment grazes AABB corner"),
         {-outside_face, 0.f, 0.f},
         {0.f, outside_face, outside_face},
         1,
         {-face, face, face}},
        {TEXT("Parallel segment just outside face"),
         {-outside_face, just_outside_face, 0.f},
         {outside_face, just_outside_face, 0.f},
         0},
        {TEXT("Stationary point on face"), {face, 0.f, 0.f}, {face, 0.f, 0.f}, 1, {face, 0.f, 0.f}},
        {TEXT("Stationary point on corner"),
         {face, face, face},
         {face, face, face},
         1,
         {face, face, face}},
        {TEXT("Stationary point just outside face"),
         {just_outside_face, 0.f, 0.f},
         {just_outside_face, 0.f, 0.f},
         0},
    };

    check_traces(checks, fixture, cases);
}

void FCollisionUniformGridTraceScenario::test_grid_boundary_traversal() {
    FVector3f const aabb_half_extents{10.f, 10.f, 10.f};
    constexpr float trace_extent{150.f};

    {
        TArray<FVector3f> const entity_locations{{-10.f, 0.f, 0.f}};
        FTraceFixture const fixture{entity_locations, aabb_half_extents};
        TArray<FExpectedTrace> const cases{
            {TEXT("Y trace lies on X cell plane"),
             {0.f, -trace_extent, 0.f},
             {0.f, trace_extent, 0.f},
             1,
             {0.f, -10.f, 0.f}},
            {TEXT("Trace ends at origin from positive X cell"),
             {trace_extent, 0.f, 0.f},
             {0.f, 0.f, 0.f},
             1,
             {0.f, 0.f, 0.f}},
            {TEXT("Trace starts at origin toward negative X cell"),
             {0.f, 0.f, 0.f},
             {-trace_extent, 0.f, 0.f},
             1,
             {0.f, 0.f, 0.f}},
        };
        check_traces(checks, fixture, cases);
    }

    {
        TArray<FVector3f> const entity_locations{{0.f, -10.f, 0.f}};
        FTraceFixture const fixture{entity_locations, aabb_half_extents};
        TArray<FExpectedTrace> const cases{
            {TEXT("X trace lies on Y cell plane"),
             {-trace_extent, 0.f, 0.f},
             {trace_extent, 0.f, 0.f},
             1,
             {-10.f, 0.f, 0.f}},
            {TEXT("Trace ends at origin from positive Y cell"),
             {0.f, trace_extent, 0.f},
             {0.f, 0.f, 0.f},
             1,
             {0.f, 0.f, 0.f}},
        };
        check_traces(checks, fixture, cases);
    }

    {
        TArray<FVector3f> const entity_locations{{0.f, 0.f, -10.f}};
        FTraceFixture const fixture{entity_locations, aabb_half_extents};
        TArray<FExpectedTrace> const cases{
            {TEXT("X trace lies on Z cell plane"),
             {-trace_extent, 0.f, 0.f},
             {trace_extent, 0.f, 0.f},
             1,
             {-10.f, 0.f, 0.f}},
            {TEXT("Trace ends at origin from positive Z cell"),
             {0.f, 0.f, trace_extent},
             {0.f, 0.f, 0.f},
             1,
             {0.f, 0.f, 0.f}},
        };
        check_traces(checks, fixture, cases);
    }

    {
        TArray<FVector3f> const entity_locations{{10.f, -10.f, 0.f}};
        FTraceFixture const fixture{entity_locations, aabb_half_extents};
        TArray<FExpectedTrace> const cases{
            {TEXT("Positive diagonal touches entity at cell edge"),
             {-trace_extent, -trace_extent, 0.f},
             {trace_extent, trace_extent, 0.f},
             1,
             {0.f, 0.f, 0.f}},
            {TEXT("Negative diagonal touches entity at cell edge"),
             {trace_extent, trace_extent, 0.f},
             {-trace_extent, -trace_extent, 0.f},
             1,
             {0.f, 0.f, 0.f}},
        };
        check_traces(checks, fixture, cases);
    }

    {
        TArray<FVector3f> const entity_locations{{10.f, -10.f, -10.f}};
        FTraceFixture const fixture{entity_locations, aabb_half_extents};
        TArray<FExpectedTrace> const cases{
            {TEXT("Three-axis diagonal touches entity at cell corner"),
             {-trace_extent, -trace_extent, -trace_extent},
             {trace_extent, trace_extent, trace_extent},
             1,
             {0.f, 0.f, 0.f}},
        };
        check_traces(checks, fixture, cases);
    }
}

void FCollisionUniformGridTraceScenario::test_short_and_near_parallel_segments() {
    FVector3f const entity_centre{};
    FVector3f const aabb_half_extents{10.f, 10.f, 10.f};
    constexpr float face{-10.f};

    TArray<FVector3f> const entity_locations{entity_centre};
    FTraceFixture const fixture{entity_locations, aabb_half_extents};
    TArray<FExpectedTrace> const cases{
        {TEXT("Short segment stops before face"), {-20.f, 0.f, 0.f}, {-10.5f, 0.f, 0.f}, 0},
        {TEXT("Short segment stops on face"),
         {-20.f, 0.f, 0.f},
         {face, 0.f, 0.f},
         1,
         {face, 0.f, 0.f}},
        {TEXT("Short segment crosses face"),
         {-10.25f, 0.f, 0.f},
         {-9.75f, 0.f, 0.f},
         1,
         {face, 0.f, 0.f}},
        {TEXT("Very short segment remains inside"),
         {0.f, 1.f, 2.f},
         {0.001f, 1.f, 2.f},
         1,
         {0.f, 1.f, 2.f}},
        {TEXT("Segment remains entirely inside"),
         {-1.f, 2.f, 3.f},
         {1.f, 2.f, 3.f},
         1,
         {-1.f, 2.f, 3.f}},
        {TEXT("Very short segment remains outside"), {20.f, 0.f, 0.f}, {20.001f, 0.f, 0.f}, 0},
        {TEXT("Segment points away from AABB"), {-20.f, 0.f, 0.f}, {-21.f, 0.f, 0.f}, 0},
    };

    check_traces(checks, fixture, cases);
}

void FCollisionUniformGridTraceScenario::test_clips_to_grid_bounds() {
    FVector3f const entity_centre{};
    FVector3f const aabb_half_extents{10.f, 10.f, 10.f};
    constexpr float grid_extent{400.f};
    constexpr float outside_grid{500.f};

    TArray<FVector3f> const entity_locations{entity_centre};
    FTraceFixture const fixture{entity_locations, aabb_half_extents};
    TArray<FExpectedTrace> const cases{
        {TEXT("X segment crosses grid from outside"),
         {-outside_grid, 0.f, 0.f},
         {outside_grid, 0.f, 0.f},
         1,
         {-10.f, 0.f, 0.f}},
        {TEXT("Reverse X segment crosses grid from outside"),
         {outside_grid, 0.f, 0.f},
         {-outside_grid, 0.f, 0.f},
         1,
         {10.f, 0.f, 0.f}},
        {TEXT("Y segment crosses grid from outside"),
         {0.f, -outside_grid, 0.f},
         {0.f, outside_grid, 0.f},
         1,
         {0.f, -10.f, 0.f}},
        {TEXT("Segment starts on negative grid boundary"),
         {-grid_extent, 0.f, 0.f},
         {0.f, 0.f, 0.f},
         1,
         {-10.f, 0.f, 0.f}},
        {TEXT("Segment starts on positive grid boundary"),
         {grid_extent, 0.f, 0.f},
         {0.f, 0.f, 0.f},
         1,
         {10.f, 0.f, 0.f}},
        {TEXT("Outside segment stops at grid before AABB"),
         {-outside_grid, 0.f, 0.f},
         {-grid_extent, 0.f, 0.f},
         0},
        {TEXT("Segment remains fully outside grid"),
         {-outside_grid, 0.f, 0.f},
         {-outside_grid - 25.f, 0.f, 0.f},
         0},
    };

    check_traces(checks, fixture, cases);
}

void FCollisionUniformGridTraceScenario::test_degenerate_aabbs() {
    FVector3f const cell_interior{25.f, 25.f, 25.f};
    constexpr float trace_offset{25.f};

    {
        FVector3f const point_half_extents{};
        TArray<FVector3f> const entity_locations{cell_interior};
        FTraceFixture const fixture{entity_locations, point_half_extents};
        TArray<FExpectedTrace> const cases{
            {TEXT("Trace crosses point AABB in cell interior"),
             {cell_interior.X - trace_offset, cell_interior.Y, cell_interior.Z},
             {cell_interior.X + trace_offset, cell_interior.Y, cell_interior.Z},
             1,
             cell_interior},
            {TEXT("Stationary trace equals point AABB"),
             cell_interior,
             cell_interior,
             1,
             cell_interior},
            {TEXT("Trace misses point AABB"),
             {cell_interior.X - trace_offset, cell_interior.Y + 1.f, cell_interior.Z},
             {cell_interior.X + trace_offset, cell_interior.Y + 1.f, cell_interior.Z},
             0},
        };
        check_traces(checks, fixture, cases);
    }

    {
        FVector3f const plane_half_extents{0.f, 10.f, 10.f};
        TArray<FVector3f> const entity_locations{cell_interior};
        FTraceFixture const fixture{entity_locations, plane_half_extents};
        TArray<FExpectedTrace> const cases{
            {TEXT("Trace crosses plane AABB"),
             {cell_interior.X - trace_offset, cell_interior.Y, cell_interior.Z},
             {cell_interior.X + trace_offset, cell_interior.Y, cell_interior.Z},
             1,
             cell_interior},
        };
        check_traces(checks, fixture, cases);
    }

    {
        FVector3f const line_half_extents{10.f, 0.f, 0.f};
        TArray<FVector3f> const entity_locations{cell_interior};
        FTraceFixture const fixture{entity_locations, line_half_extents};
        TArray<FExpectedTrace> const cases{
            {TEXT("Trace crosses line AABB"),
             {cell_interior.X, cell_interior.Y - trace_offset, cell_interior.Z},
             {cell_interior.X, cell_interior.Y + trace_offset, cell_interior.Z},
             1,
             cell_interior},
        };
        check_traces(checks, fixture, cases);
    }

    {
        FVector3f const origin{};
        FVector3f const point_half_extents{};
        TArray<FVector3f> const entity_locations{origin};
        FTraceFixture const fixture{entity_locations, point_half_extents};
        TArray<FExpectedTrace> const cases{
            {TEXT("Trace crosses point AABB on grid corner"),
             {-trace_offset, 0.f, 0.f},
             {trace_offset, 0.f, 0.f},
             1,
             origin},
            {TEXT("Stationary trace equals point AABB on grid corner"), origin, origin, 1, origin},
        };
        check_traces(checks, fixture, cases);
    }
}

void FCollisionUniformGridTraceScenario::test_cross_cell_nearest_hit() {
    FVector3f const positive_entity_centre{150.f, 0.f, 0.f};
    FVector3f const negative_entity_centre{-150.f, 0.f, 0.f};
    FVector3f const aabb_half_extents{10.f, 10.f, 10.f};
    constexpr float trace_extent{300.f};

    {
        TArray<FVector3f> const entity_locations{
            positive_entity_centre,
            negative_entity_centre,
        };
        FTraceFixture const fixture{entity_locations, aabb_half_extents};
        TArray<FExpectedTrace> const cases{
            {TEXT("Positive trace returns nearest entity in earlier cell"),
             {-trace_extent, 0.f, 0.f},
             {trace_extent, 0.f, 0.f},
             1,
             {-160.f, 0.f, 0.f},
             1},
            {TEXT("Negative trace returns nearest entity in earlier cell"),
             {trace_extent, 0.f, 0.f},
             {-trace_extent, 0.f, 0.f},
             1,
             {160.f, 0.f, 0.f},
             0},
        };
        check_traces(checks, fixture, cases);
    }

    {
        FVector3f const wide_aabb_half_extents{160.f, 10.f, 10.f};
        TArray<FVector3f> const entity_locations{{0.f, 0.f, 0.f}};
        FTraceFixture const fixture{entity_locations, wide_aabb_half_extents};
        TArray<FExpectedTrace> const cases{
            {TEXT("AABB repeated across cells returns one stable contact"),
             {-trace_extent, 0.f, 0.f},
             {trace_extent, 0.f, 0.f},
             1,
             {-160.f, 0.f, 0.f}},
        };
        check_traces(checks, fixture, cases);
    }
}

void FCollisionUniformGridTraceScenario::run() {
    TestCommandBuilder.Do([this] {
        switch (scenario_) {
            case ECollisionUniformGridTraceScenario::HitsAndMisses:
                test_hits_and_misses();
                break;
            case ECollisionUniformGridTraceScenario::StopsAtEndpoint:
                test_stops_at_endpoint();
                break;
            case ECollisionUniformGridTraceScenario::ReturnsNearestHit:
                test_returns_nearest_hit();
                break;
            case ECollisionUniformGridTraceScenario::HandlesZeroLengthTraces:
                test_handles_zero_length_traces();
                break;
            case ECollisionUniformGridTraceScenario::IncludesNegativeEndpointBoundary:
                test_includes_negative_endpoint_boundary();
                break;
            case ECollisionUniformGridTraceScenario::AppliesAABBCentre:
                test_applies_aabb_centre();
                break;
            case ECollisionUniformGridTraceScenario::AxisParallelAndOrigin:
                test_axis_parallel_and_origin();
                break;
            case ECollisionUniformGridTraceScenario::SurfaceContacts:
                test_surface_contacts();
                break;
            case ECollisionUniformGridTraceScenario::GridBoundaryTraversal:
                test_grid_boundary_traversal();
                break;
            case ECollisionUniformGridTraceScenario::ShortAndNearParallelSegments:
                test_short_and_near_parallel_segments();
                break;
            case ECollisionUniformGridTraceScenario::ClipsToGridBounds:
                test_clips_to_grid_bounds();
                break;
            case ECollisionUniformGridTraceScenario::DegenerateAABBs:
                test_degenerate_aabbs();
                break;
            case ECollisionUniformGridTraceScenario::CrossCellNearestHit:
                test_cross_cell_nearest_hit();
                break;
        }

        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    });
}
}
