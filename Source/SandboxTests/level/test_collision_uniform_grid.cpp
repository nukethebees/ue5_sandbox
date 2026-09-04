#include "test_collision_uniform_grid_scenario.h"

#include <SandboxTests/support/SimulationTestAssets.h>
#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/TestCollisionActor.h>

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

#include <cmath>
#include <Components/BoxComponent.h>
#include <Components/InstancedStaticMeshComponent.h>
#include <Engine/World.h>
#include <limits>

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
                  FVector3f const aabb_centre = FVector3f::ZeroVector,
                  FIntVector3 const fixture_grid_dims = trace_grid_dims,
                  FVector3f const fixture_cell_dims = trace_cell_dims,
                  TConstArrayView<ETestEntityType> const fixture_entity_types = {}) {
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
        if (fixture_entity_types.IsEmpty()) {
            entity_types.Init(ETestEntityType::CapitalShip, count);
        } else {
            check(fixture_entity_types.Num() == count);
            entity_types.Append(fixture_entity_types);
        }
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

        set_entity_aabb(ETestEntityType::CapitalShip, aabb_centre, half_extents);

        grid.set_grid_dims(fixture_grid_dims);
        grid.set_cell_dims(fixture_cell_dims);
        grid.set_entity_registry(registry);
        grid.rebuild_grid(aabbs);
    }

    void set_entity_aabb(ETestEntityType const entity_type,
                         FVector3f const centre,
                         FVector3f const half_extents) {
        auto const aabb_index{std::to_underlying(entity_type)};
        aabbs.centre_xs[aabb_index] = centre.X;
        aabbs.centre_ys[aabb_index] = centre.Y;
        aabbs.centre_zs[aabb_index] = centre.Z;
        aabbs.half_extent_xs[aabb_index] = half_extents.X;
        aabbs.half_extent_ys[aabb_index] = half_extents.Y;
        aabbs.half_extent_zs[aabb_index] = half_extents.Z;
    }

    void update_entities(TConstArrayView<FVector3f> const locations,
                         TConstArrayView<uint8> const alive) {
        auto const count{handles.Num()};
        check(locations.Num() == count);
        check(alive.Num() == count);

        FVectors3f registry_locations;
        FVectors3f velocities;
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
        radii.Init(0.f, count);
        healths.Init(1, count);
        teams.Init(ETestTeam::Blue, count);
        entity_types.Init(ETestEntityType::CapitalShip, count);

        FTestEntityRegistry::EntityData::ConstView const entity_data{
            .locations = registry_locations.get_const_view(),
            .velocities = velocities.get_const_view(),
            .radii = radii,
            .healths = healths,
            .teams = teams,
            .entity_types = entity_types,
            .alive = alive,
        };
        FTestEntityRegistry::ConstView const updates{
            .indices = handles,
            .data = entity_data,
        };
        EntityDeathInfo death_info;
        for (int32 i{}; i < count; ++i) {
            if (alive[i] == 0 && registry.get_alive(handles[i])) {
                death_info.add(ETestDeathReason::Unknown, handles[i]);
            }
        }
        registry.queue_entity_updates(updates, death_info);
        registry.commit_updates();
        registry.end_tick();
        grid.rebuild_grid(aabbs);
    }

    auto add_entity(FVector3f const location) -> FRegistryEntityHandle {
        FVectors3f registry_locations;
        FVectors3f velocities;
        registry_locations.add(location);
        velocities.add(FVector3f::ZeroVector);

        TArray<float> const radii{0.f};
        TArray<int32> const healths{1};
        TArray<ETestTeam> const teams{ETestTeam::Blue};
        TArray<ETestEntityType> const entity_types{ETestEntityType::CapitalShip};
        TArray<uint8> const alive{uint8{1}};
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
        grid.rebuild_grid(aabbs);
        return spawned.registry_handles[0];
    }

    FTestEntityRegistry registry;
    ioj::CollisionUniformGrid grid;
    TArray<FRegistryEntityHandle> handles;
    ioj::FEntityAABBs aabbs;
};

auto reference_trace_aabb(FVector3f const start,
                          FVector3f const end,
                          FVector3f const aabb_min,
                          FVector3f const aabb_max) -> float {
    constexpr auto no_hit{std::numeric_limits<float>::infinity()};
    auto const delta{end - start};
    float t_min{};
    float t_max{1.f};

    for (int32 axis{}; axis < 3; ++axis) {
        auto const axis_delta{delta[axis]};
        if (axis_delta == 0.f) {
            if (start[axis] < aabb_min[axis] || start[axis] > aabb_max[axis]) {
                return no_hit;
            }
            continue;
        }

        auto t1{(aabb_min[axis] - start[axis]) / axis_delta};
        auto t2{(aabb_max[axis] - start[axis]) / axis_delta};
        if (t1 > t2) {
            Swap(t1, t2);
        }

        t_min = FMath::Max(t_min, t1);
        t_max = FMath::Min(t_max, t2);
        if (t_min > t_max) {
            return no_hit;
        }
    }

    return t_min;
}

auto run_traces(FTraceFixture const& fixture,
                TConstArrayView<FVector3f> const starts,
                TConstArrayView<FVector3f> const ends,
                TConstArrayView<FRegistryEntityHandle> const ignored_entities = {}) -> FTraceHits {
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
    fixture.grid.trace_aabbs(traces.get_const_view(), hits.get_view(), ignored_entities);
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
        auto const aabb_index{std::to_underlying(registered_type)};
        auto const entity_location{registry.get_location(handle)};
        auto const local_aabb_centre{entity_aabbs.get_centre(aabb_index)};
        auto const half_extents{entity_aabbs.get_half_extents(aabb_index)};
        auto const world_aabb_centre{entity_location + local_aabb_centre};
        auto const [min_coord, max_coord]{grid.to_cell_coord_bounds(
            world_aabb_centre - half_extents, world_aabb_centre + half_extents)};

        auto const is_in_bounds{grid.is_cell_coord_in_bounds(min_coord, max_coord)};
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

    TArray<ETestEntityType> const entity_types{
        ETestEntityType::PlayerShip,
        ETestEntityType::Turret,
        ETestEntityType::CapitalShip,
        ETestEntityType::CapitalShipFighter,
        ETestEntityType::TubeSpinner,
    };
    TArray<FVector3f> const mixed_locations{
        {-300.f, 0.f, 0.f},
        {-150.f, 0.f, 0.f},
        {0.f, 0.f, 0.f},
        {150.f, 0.f, 0.f},
        {300.f, 0.f, 0.f},
    };
    TArray<FVector3f> const local_centres{
        {5.f, -20.f, 3.f},
        {-6.f, -10.f, -2.f},
        {0.f, 0.f, 0.f},
        {8.f, 10.f, -4.f},
        {-9.f, 20.f, 5.f},
    };
    TArray<FVector3f> const half_extents{
        {4.f, 5.f, 6.f},
        {7.f, 8.f, 9.f},
        {10.f, 11.f, 12.f},
        {13.f, 14.f, 15.f},
        {16.f, 17.f, 18.f},
    };
    FTraceFixture mixed_fixture{mixed_locations,
                                half_extents[ioj::FEntityAABBs::capital_ship_index],
                                local_centres[ioj::FEntityAABBs::capital_ship_index],
                                trace_grid_dims,
                                trace_cell_dims,
                                entity_types};
    auto const entity_type_count{entity_types.Num()};
    for (int32 i{}; i < entity_type_count; ++i) {
        mixed_fixture.set_entity_aabb(entity_types[i], local_centres[i], half_extents[i]);
    }
    mixed_fixture.grid.rebuild_grid(mixed_fixture.aabbs);

    TArray<FVector3f> mixed_starts;
    TArray<FVector3f> mixed_ends;
    mixed_starts.Reserve(entity_type_count);
    mixed_ends.Reserve(entity_type_count);
    for (int32 i{}; i < entity_type_count; ++i) {
        auto const world_centre{mixed_locations[i] + local_centres[i]};
        mixed_starts.Add(world_centre - FVector3f{0.f, 50.f, 0.f});
        mixed_ends.Add(world_centre + FVector3f{0.f, 50.f, 0.f});
    }

    auto const mixed_hits{run_traces(mixed_fixture, mixed_starts, mixed_ends)};
    for (int32 i{}; i < entity_type_count; ++i) {
        checks.are_equal(
            uint8{1}, mixed_hits.hits[i], TEXT("Mixed entity-type trace records a hit"), i);
        if (mixed_hits.hits[i] == 0) {
            continue;
        }

        checks.are_equal(mixed_fixture.handles[i],
                         mixed_hits.entities[i],
                         TEXT("Mixed entity-type trace resolves its entity"),
                         i);
        auto const world_centre{mixed_locations[i] + local_centres[i]};
        auto const expected_type_contact{world_centre - FVector3f{0.f, half_extents[i].Y, 0.f}};
        checks.dist_zero(expected_type_contact,
                         mixed_hits.locations[i],
                         hit_location_tolerance,
                         TEXT("Mixed entity-type trace applies its AABB row"),
                         i);
    }
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
    constexpr float distant_outside_grid{20000.f};

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
        {TEXT("Distant X segment crosses grid"),
         {-distant_outside_grid, 0.f, 0.f},
         {distant_outside_grid, 0.f, 0.f},
         1,
         {-10.f, 0.f, 0.f}},
        {TEXT("Distant segment remains outside negative grid boundary"),
         {-distant_outside_grid, 0.f, 0.f},
         {-distant_outside_grid + 1000.f, 0.f, 0.f},
         0},
        {TEXT("Distant segment remains outside positive grid boundary"),
         {distant_outside_grid - 1000.f, 0.f, 0.f},
         {distant_outside_grid, 0.f, 0.f},
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

void FCollisionUniformGridTraceScenario::test_varied_grid_geometry() {
    FIntVector3 const fixture_grid_dims{5, 7, 3};
    FVector3f const fixture_cell_dims{80.f, 125.f, 250.f};
    FVector3f const entity_centre{25.f, -30.f, 40.f};
    FVector3f const aabb_half_extents{15.f, 20.f, 25.f};

    TArray<FVector3f> const entity_locations{entity_centre};
    FTraceFixture const fixture{entity_locations,
                                aabb_half_extents,
                                FVector3f::ZeroVector,
                                fixture_grid_dims,
                                fixture_cell_dims};
    TArray<FExpectedTrace> const cases{
        {TEXT("Positive X trace in nonuniform grid"),
         {-190.f, -30.f, 40.f},
         {190.f, -30.f, 40.f},
         1,
         {10.f, -30.f, 40.f}},
        {TEXT("Negative Y trace in nonuniform grid"),
         {25.f, 300.f, 40.f},
         {25.f, -300.f, 40.f},
         1,
         {25.f, -10.f, 40.f}},
        {TEXT("Positive Z trace in nonuniform grid"),
         {25.f, -30.f, -300.f},
         {25.f, -30.f, 300.f},
         1,
         {25.f, -30.f, 15.f}},
        {TEXT("Nonuniform-grid trace outside AABB slab"),
         {-190.f, -5.f, 40.f},
         {190.f, -5.f, 40.f},
         0},
    };
    check_traces(checks, fixture, cases);

    FVector3f const boundary_entity_centre{-55.f, -30.f, 40.f};
    TArray<FVector3f> const boundary_entity_locations{boundary_entity_centre};
    FTraceFixture const boundary_fixture{boundary_entity_locations,
                                         aabb_half_extents,
                                         FVector3f::ZeroVector,
                                         fixture_grid_dims,
                                         fixture_cell_dims};
    TArray<FExpectedTrace> const boundary_cases{
        {TEXT("Trace follows cell boundary in odd nonuniform grid"),
         {-40.f, -300.f, 40.f},
         {-40.f, 300.f, 40.f},
         1,
         {-40.f, -50.f, 40.f}},
    };
    check_traces(checks, boundary_fixture, boundary_cases);

    FIntVector3 const single_cell_grid_dims{1, 1, 1};
    FVector3f const single_cell_dims{100.f, 120.f, 140.f};
    FVector3f const single_cell_half_extents{10.f, 10.f, 10.f};
    TArray<FVector3f> const single_cell_locations{FVector3f::ZeroVector};
    FTraceFixture const single_cell_fixture{single_cell_locations,
                                            single_cell_half_extents,
                                            FVector3f::ZeroVector,
                                            single_cell_grid_dims,
                                            single_cell_dims};
    checks.are_equal(
        1, single_cell_fixture.grid.num_cells(), TEXT("Single-cell grid has one cell"));
    checks.are_equal(1,
                     single_cell_fixture.grid.get_cell_entities(FIntVector3::ZeroValue).Num(),
                     TEXT("Single-cell grid contains its entity"));

    TArray<FExpectedTrace> const single_cell_cases{
        {TEXT("Diagonal trace crosses single-cell grid"),
         {-100.f, -100.f, -100.f},
         {100.f, 100.f, 100.f},
         1,
         {-10.f, -10.f, -10.f}},
        {TEXT("Stationary trace hits inside single-cell grid"),
         FVector3f::ZeroVector,
         FVector3f::ZeroVector,
         1,
         FVector3f::ZeroVector},
        {TEXT("Trace enters single-cell grid from positive boundary"),
         {50.f, 0.f, 0.f},
         FVector3f::ZeroVector,
         1,
         {10.f, 0.f, 0.f}},
        {TEXT("Trace along excluded single-cell positive boundary"),
         {50.f, -20.f, 0.f},
         {50.f, 20.f, 0.f},
         0},
    };
    check_traces(checks, single_cell_fixture, single_cell_cases);
}

void FCollisionUniformGridTraceScenario::test_boundary_precision() {
    FVector3f const entity_centre{};
    FVector3f const aabb_half_extents{10.f, 10.f, 10.f};
    constexpr float face{10.f};
    constexpr float negative_face{-face};
    constexpr float trace_extent{20.f};
    auto const inside_face{std::nextafter(face, 0.f)};
    auto const outside_face{std::nextafter(face, std::numeric_limits<float>::infinity())};
    auto const before_negative_face{
        std::nextafter(negative_face, -std::numeric_limits<float>::infinity())};
    auto const after_negative_face{std::nextafter(negative_face, 0.f)};

    TArray<FVector3f> const entity_locations{entity_centre};
    FTraceFixture const fixture{entity_locations, aabb_half_extents};
    TArray<FExpectedTrace> const cases{
        {TEXT("Parallel trace exactly on face"),
         {-trace_extent, face, 0.f},
         {trace_extent, face, 0.f},
         1,
         {negative_face, face, 0.f}},
        {TEXT("Parallel trace one float inside face"),
         {-trace_extent, inside_face, 0.f},
         {trace_extent, inside_face, 0.f},
         1,
         {negative_face, inside_face, 0.f}},
        {TEXT("Parallel trace one float outside face"),
         {-trace_extent, outside_face, 0.f},
         {trace_extent, outside_face, 0.f},
         0},
        {TEXT("Endpoint one float before face"),
         {-trace_extent, 0.f, 0.f},
         {before_negative_face, 0.f, 0.f},
         0},
        {TEXT("Endpoint exactly on face"),
         {-trace_extent, 0.f, 0.f},
         {negative_face, 0.f, 0.f},
         1,
         {negative_face, 0.f, 0.f}},
        {TEXT("Endpoint one float beyond face"),
         {-trace_extent, 0.f, 0.f},
         {after_negative_face, 0.f, 0.f},
         1,
         {negative_face, 0.f, 0.f}},
    };
    check_traces(checks, fixture, cases);
}

void FCollisionUniformGridTraceScenario::test_rebuild_lifecycle() {
    FVector3f const initial_location{-150.f, 0.f, 0.f};
    FVector3f const moved_location{150.f, 0.f, 0.f};
    FVector3f const aabb_half_extents{10.f, 10.f, 10.f};
    constexpr float trace_offset{25.f};

    TArray<FVector3f> const initial_locations{initial_location};
    FTraceFixture fixture{initial_locations, aabb_half_extents};
    for (int32 rebuild{}; rebuild < 4; ++rebuild) {
        fixture.grid.rebuild_grid(fixture.aabbs);
    }

    TArray<FExpectedTrace> const initial_cases{
        {TEXT("Repeated rebuild retains entity"),
         {initial_location.X - trace_offset, 0.f, 0.f},
         {initial_location.X + trace_offset, 0.f, 0.f},
         1,
         {initial_location.X - aabb_half_extents.X, 0.f, 0.f}},
    };
    check_traces(checks, fixture, initial_cases);

    TArray<FVector3f> const moved_locations{moved_location};
    TArray<uint8> const alive{uint8{1}};
    fixture.update_entities(moved_locations, alive);
    TArray<FExpectedTrace> const moved_cases{
        {TEXT("Moved entity is removed from old cells"),
         {initial_location.X - trace_offset, 0.f, 0.f},
         {initial_location.X + trace_offset, 0.f, 0.f},
         0},
        {TEXT("Moved entity is added to new cells"),
         {moved_location.X - trace_offset, 0.f, 0.f},
         {moved_location.X + trace_offset, 0.f, 0.f},
         1,
         {moved_location.X - aabb_half_extents.X, 0.f, 0.f}},
    };
    check_traces(checks, fixture, moved_cases);

    TArray<uint8> const dead{uint8{0}};
    fixture.update_entities(moved_locations, dead);
    TArray<FExpectedTrace> const dead_cases{
        {TEXT("Dead entity is removed on rebuild"),
         {moved_location.X - trace_offset, 0.f, 0.f},
         {moved_location.X + trace_offset, 0.f, 0.f},
         0},
    };
    check_traces(checks, fixture, dead_cases);

    FVector3f const replacement_location{250.f, 0.f, 0.f};
    auto const old_handle{fixture.handles[0]};
    auto const replacement_handle{fixture.add_entity(replacement_location)};
    checks.are_equal(old_handle.index,
                     replacement_handle.index,
                     TEXT("Replacement entity reuses dead registry slot"));
    checks.is_true(old_handle.generation != replacement_handle.generation,
                   TEXT("Replacement entity advances registry generation"));
    checks.is_true(fixture.registry.is_stale(old_handle),
                   TEXT("Reused collision handle becomes stale"));

    TArray<FVector3f> const replacement_starts{
        {replacement_location.X - trace_offset, 0.f, 0.f},
    };
    TArray<FVector3f> const replacement_ends{
        {replacement_location.X + trace_offset, 0.f, 0.f},
    };
    auto const replacement_hits{run_traces(fixture, replacement_starts, replacement_ends)};
    checks.are_equal(
        uint8{1}, replacement_hits.hits[0], TEXT("Replacement entity is added on rebuild"));
    if (replacement_hits.hits[0] != 0) {
        checks.are_equal(replacement_handle,
                         replacement_hits.entities[0],
                         TEXT("Trace resolves replacement generation"));
    }

    TArray<FVector3f> const sparse_locations{
        {-250.f, 0.f, 0.f},
        {0.f, 0.f, 0.f},
        {250.f, 0.f, 0.f},
    };
    FTraceFixture sparse_fixture{sparse_locations, aabb_half_extents};
    TArray<uint8> const sparse_alive{uint8{1}, uint8{0}, uint8{1}};
    sparse_fixture.update_entities(sparse_locations, sparse_alive);
    TArray<FExpectedTrace> const sparse_cases{
        {TEXT("Live entity before dead slot remains traceable"),
         {-250.f - trace_offset, 0.f, 0.f},
         {-250.f + trace_offset, 0.f, 0.f},
         1,
         {-250.f - aabb_half_extents.X, 0.f, 0.f},
         0},
        {TEXT("Dead middle registry slot is omitted"),
         {-trace_offset, 0.f, 0.f},
         {trace_offset, 0.f, 0.f},
         0},
        {TEXT("Live entity after dead slot remains traceable"),
         {250.f - trace_offset, 0.f, 0.f},
         {250.f + trace_offset, 0.f, 0.f},
         1,
         {250.f - aabb_half_extents.X, 0.f, 0.f},
         2},
    };
    check_traces(checks, sparse_fixture, sparse_cases);

    FVector3f const sparse_replacement_location{0.f, 200.f, 0.f};
    auto const sparse_old_handle{sparse_fixture.handles[1]};
    auto const sparse_replacement_handle{sparse_fixture.add_entity(sparse_replacement_location)};
    checks.are_equal(sparse_old_handle.index,
                     sparse_replacement_handle.index,
                     TEXT("Sparse replacement reuses middle registry slot"));
    checks.is_true(sparse_old_handle.generation != sparse_replacement_handle.generation,
                   TEXT("Sparse replacement advances middle registry generation"));

    TArray<FVector3f> const sparse_replacement_starts{
        sparse_replacement_location - FVector3f{0.f, trace_offset, 0.f},
    };
    TArray<FVector3f> const sparse_replacement_ends{
        sparse_replacement_location + FVector3f{0.f, trace_offset, 0.f},
    };
    auto const sparse_replacement_hits{
        run_traces(sparse_fixture, sparse_replacement_starts, sparse_replacement_ends)};
    checks.are_equal(uint8{1},
                     sparse_replacement_hits.hits[0],
                     TEXT("Sparse replacement remains traceable beside surviving entities"));
    if (sparse_replacement_hits.hits[0] != 0) {
        checks.are_equal(sparse_replacement_handle,
                         sparse_replacement_hits.entities[0],
                         TEXT("Sparse replacement trace resolves new generation"));
    }
}

void FCollisionUniformGridTraceScenario::test_deterministic_reference_sweep() {
    TStaticArray<int32, 3> const seeds{0x51A8B3, 0x19C0DE, 0x7A11CE};
    constexpr int32 entity_count{32};
    constexpr int32 trace_count{512};
    constexpr float entity_extent{320.f};
    constexpr float trace_extent{800.f};
    FVector3f const targeted_trace_extent{1000.f, 1000.f, 1000.f};
    FVector3f const local_aabb_centre{3.f, -5.f, 7.f};
    FVector3f const aabb_half_extents{7.f, 11.f, 13.f};

    auto const run_sweep{[this, local_aabb_centre, aabb_half_extents, targeted_trace_extent](
                             int32 const seed, int32 const case_offset) {
        FRandomStream random{seed};
        auto const random_range{[&random](float const extent) {
            return static_cast<float>(random.FRandRange(-extent, extent));
        }};
        TArray<FVector3f> entity_locations;
        entity_locations.Reserve(entity_count);
        for (int32 i{}; i < entity_count; ++i) {
            entity_locations.Add({random_range(entity_extent),
                                  random_range(entity_extent),
                                  random_range(entity_extent)});
        }

        FTraceFixture const fixture{entity_locations, aabb_half_extents, local_aabb_centre};
        TArray<FVector3f> starts;
        TArray<FVector3f> ends;
        starts.Reserve(trace_count);
        ends.Reserve(trace_count);
        for (int32 i{}; i < entity_count; ++i) {
            auto const entity_location{entity_locations[i]};
            auto const world_centre{entity_location + local_aabb_centre};
            auto const start_offset{i % 2 == 0 ? -targeted_trace_extent : targeted_trace_extent};
            starts.Add(world_centre + start_offset);
            ends.Add(world_centre - start_offset);
        }
        for (int32 i{entity_count}; i < trace_count; ++i) {
            starts.Add({random_range(trace_extent),
                        random_range(trace_extent),
                        random_range(trace_extent)});
            ends.Add({random_range(trace_extent),
                      random_range(trace_extent),
                      random_range(trace_extent)});
        }

        auto const hits{run_traces(fixture, starts, ends)};
        int32 expected_hit_count{};
        for (int32 i_trace{}; i_trace < trace_count; ++i_trace) {
            auto nearest_t{std::numeric_limits<float>::infinity()};
            int32 nearest_entity{-1};
            for (int32 i_entity{}; i_entity < entity_count; ++i_entity) {
                auto const world_centre{entity_locations[i_entity] + local_aabb_centre};
                auto const hit_t{reference_trace_aabb(starts[i_trace],
                                                      ends[i_trace],
                                                      world_centre - aabb_half_extents,
                                                      world_centre + aabb_half_extents)};
                if (hit_t < nearest_t) {
                    nearest_t = hit_t;
                    nearest_entity = i_entity;
                }
            }

            auto const expected_hit{uint8{FMath::IsFinite(nearest_t)}};
            expected_hit_count += expected_hit;
            auto const case_index{case_offset + i_trace};
            checks.are_equal(expected_hit,
                             hits.hits[i_trace],
                             TEXT("Reference sweep trace has expected hit flag"),
                             case_index);
            if (expected_hit == 0 || hits.hits[i_trace] == 0) {
                continue;
            }

            checks.are_equal(fixture.handles[nearest_entity],
                             hits.entities[i_trace],
                             TEXT("Reference sweep trace resolves nearest entity"),
                             case_index);
            auto const expected_location{FMath::Lerp(starts[i_trace], ends[i_trace], nearest_t)};
            checks.dist_zero(expected_location,
                             hits.locations[i_trace],
                             hit_location_tolerance,
                             TEXT("Reference sweep trace resolves nearest location"),
                             case_index);
        }

        checks.is_true(expected_hit_count >= entity_count,
                       TEXT("Reference sweep contains targeted hits"));
        checks.is_true(expected_hit_count < trace_count, TEXT("Reference sweep contains misses"));
    }};

    auto const seed_count{seeds.Num()};
    for (int32 i{}; i < seed_count; ++i) {
        run_sweep(seeds[i], i * trace_count);
    }
}

void FCollisionUniformGridTraceScenario::test_invariance_properties() {
    FVector3f const aabb_half_extents{18.f, 22.f, 15.f};
    FVector3f const local_aabb_centre{4.f, -3.f, 5.f};
    TArray<FVector3f> const entity_locations{
        {-220.f, -40.f, 10.f},
        {35.f, 70.f, -20.f},
        {210.f, -120.f, 80.f},
    };
    TArray<FVector3f> const starts{
        {-280.f, -43.f, 15.f},
        {-21.f, 67.f, -15.f},
        {154.f, -123.f, 85.f},
        {-280.f, 150.f, 15.f},
        {35.f, -200.f, -15.f},
        {-300.f, -300.f, -300.f},
    };
    TArray<FVector3f> const ends{
        {-160.f, -43.f, 15.f},
        {99.f, 67.f, -15.f},
        {274.f, -123.f, 85.f},
        {-160.f, 150.f, 15.f},
        {35.f, -100.f, -15.f},
        {-250.f, -250.f, -250.f},
    };

    FTraceFixture const baseline_fixture{entity_locations, aabb_half_extents, local_aabb_centre};
    auto const baseline_hits{run_traces(baseline_fixture, starts, ends)};
    auto const compare_same_entity_order{[this, &baseline_hits](FTraceHits const& candidate_hits,
                                                                TCHAR const* const description) {
        auto const count{baseline_hits.num()};
        for (int32 i{}; i < count; ++i) {
            checks.are_equal(baseline_hits.hits[i], candidate_hits.hits[i], description, i);
            if (baseline_hits.hits[i] == 0 || candidate_hits.hits[i] == 0) {
                continue;
            }
            checks.are_equal(
                baseline_hits.entities[i].index, candidate_hits.entities[i].index, description, i);
            checks.dist_zero(baseline_hits.locations[i],
                             candidate_hits.locations[i],
                             hit_location_tolerance,
                             description,
                             i);
        }
    }};

    FTraceFixture const coarse_fixture{
        entity_locations, aabb_half_extents, local_aabb_centre, {4, 4, 4}, {200.f, 200.f, 200.f}};
    auto const coarse_hits{run_traces(coarse_fixture, starts, ends)};
    compare_same_entity_order(coarse_hits, TEXT("Coarse grid preserves trace results"));

    FTraceFixture const fine_fixture{
        entity_locations, aabb_half_extents, local_aabb_centre, {16, 16, 16}, {50.f, 50.f, 50.f}};
    auto const fine_hits{run_traces(fine_fixture, starts, ends)};
    compare_same_entity_order(fine_hits, TEXT("Fine grid preserves trace results"));

    TStaticArray<int32, 6> const permutation{4, 1, 5, 0, 3, 2};
    TArray<FVector3f> permuted_starts;
    TArray<FVector3f> permuted_ends;
    for (auto const source_index : permutation) {
        permuted_starts.Add(starts[source_index]);
        permuted_ends.Add(ends[source_index]);
    }
    auto const permuted_hits{run_traces(baseline_fixture, permuted_starts, permuted_ends)};
    auto const trace_count{permutation.Num()};
    for (int32 i{}; i < trace_count; ++i) {
        auto const source_index{permutation[i]};
        checks.are_equal(baseline_hits.hits[source_index],
                         permuted_hits.hits[i],
                         TEXT("Trace permutation preserves hit flag"),
                         i);
        if (baseline_hits.hits[source_index] == 0 || permuted_hits.hits[i] == 0) {
            continue;
        }
        checks.are_equal(baseline_hits.entities[source_index],
                         permuted_hits.entities[i],
                         TEXT("Trace permutation preserves entity"),
                         i);
        checks.dist_zero(baseline_hits.locations[source_index],
                         permuted_hits.locations[i],
                         hit_location_tolerance,
                         TEXT("Trace permutation preserves location"),
                         i);
    }

    TArray<FVector3f> const reversed_entity_locations{
        entity_locations[2],
        entity_locations[1],
        entity_locations[0],
    };
    FTraceFixture const reversed_fixture{
        reversed_entity_locations, aabb_half_extents, local_aabb_centre};
    auto const reversed_hits{run_traces(reversed_fixture, starts, ends)};
    for (int32 i{}; i < trace_count; ++i) {
        checks.are_equal(baseline_hits.hits[i],
                         reversed_hits.hits[i],
                         TEXT("Entity insertion order preserves hit flag"),
                         i);
        if (baseline_hits.hits[i] != 0 && reversed_hits.hits[i] != 0) {
            checks.dist_zero(baseline_hits.locations[i],
                             reversed_hits.locations[i],
                             hit_location_tolerance,
                             TEXT("Entity insertion order preserves nearest location"),
                             i);
        }
    }

    FVector3f const translation{100.f, 0.f, 0.f};
    TArray<FVector3f> translated_entity_locations;
    TArray<FVector3f> translated_starts;
    TArray<FVector3f> translated_ends;
    for (auto const location : entity_locations) {
        translated_entity_locations.Add(location + translation);
    }
    for (int32 i{}; i < trace_count; ++i) {
        translated_starts.Add(starts[i] + translation);
        translated_ends.Add(ends[i] + translation);
    }
    FTraceFixture const translated_fixture{
        translated_entity_locations, aabb_half_extents, local_aabb_centre};
    auto const translated_hits{run_traces(translated_fixture, translated_starts, translated_ends)};
    for (int32 i{}; i < trace_count; ++i) {
        checks.are_equal(baseline_hits.hits[i],
                         translated_hits.hits[i],
                         TEXT("Whole-cell translation preserves hit flag"),
                         i);
        if (baseline_hits.hits[i] != 0 && translated_hits.hits[i] != 0) {
            checks.are_equal(baseline_hits.entities[i].index,
                             translated_hits.entities[i].index,
                             TEXT("Whole-cell translation preserves entity"),
                             i);
            checks.dist_zero(baseline_hits.locations[i] + translation,
                             translated_hits.locations[i],
                             hit_location_tolerance,
                             TEXT("Whole-cell translation preserves location"),
                             i);
        }
    }
}

void FCollisionUniformGridTraceScenario::test_empty_batches_and_output_reuse() {
    FVector3f const aabb_half_extents{10.f, 10.f, 10.f};
    TArray<FVector3f> const no_entities;
    FTraceFixture const empty_fixture{no_entities, aabb_half_extents};
    TArray<FVector3f> const no_traces;
    auto const no_results{run_traces(empty_fixture, no_traces, no_traces)};
    checks.are_equal(0, no_results.num(), TEXT("Empty grid accepts empty trace batch"));

    TArray<FVector3f> const miss_starts{{-20.f, 0.f, 0.f}, {0.f, -20.f, 0.f}};
    TArray<FVector3f> const miss_ends{{20.f, 0.f, 0.f}, {0.f, 20.f, 0.f}};
    auto const empty_grid_hits{run_traces(empty_fixture, miss_starts, miss_ends)};
    checks.are_equal(uint8{0}, empty_grid_hits.hits[0], TEXT("Empty grid misses first trace"));
    checks.are_equal(uint8{0}, empty_grid_hits.hits[1], TEXT("Empty grid misses second trace"));

    TArray<FVector3f> const entity_locations{FVector3f::ZeroVector};
    FTraceFixture const populated_fixture{entity_locations, aabb_half_extents};
    auto const empty_batch_hits{run_traces(populated_fixture, no_traces, no_traces)};
    checks.are_equal(0, empty_batch_hits.num(), TEXT("Populated grid accepts empty trace batch"));

    FTraceHits reused_hits;
    reused_hits.add_defaulted(1);
    FLineTraces hit_trace;
    hit_trace.starts.add({-20.f, 0.f, 0.f});
    hit_trace.ends.add({20.f, 0.f, 0.f});
    populated_fixture.grid.trace_aabbs(hit_trace.get_const_view(), reused_hits.get_view());
    checks.are_equal(uint8{1}, reused_hits.hits[0], TEXT("Reused output initially records hit"));

    FLineTraces miss_trace;
    miss_trace.starts.add({-20.f, 20.f, 0.f});
    miss_trace.ends.add({20.f, 20.f, 0.f});
    populated_fixture.grid.trace_aabbs(miss_trace.get_const_view(), reused_hits.get_view());
    checks.are_equal(uint8{0}, reused_hits.hits[0], TEXT("Reused output clears stale hit flag"));

    populated_fixture.grid.trace_aabbs(hit_trace.get_const_view(), reused_hits.get_view());
    checks.are_equal(uint8{1}, reused_hits.hits[0], TEXT("Reused output records later hit"));
    checks.are_equal(populated_fixture.handles[0],
                     reused_hits.entities[0],
                     TEXT("Reused output records later entity"));
}

void FCollisionUniformGridTraceScenario::test_dense_and_wide_aabbs() {
    constexpr int32 dense_entity_count{2048};
    FVector3f const dense_location{25.f, 25.f, 25.f};
    FVector3f const dense_half_extents{1.f, 1.f, 1.f};
    TArray<FVector3f> dense_locations;
    dense_locations.Init(dense_location, dense_entity_count);
    FTraceFixture const dense_fixture{dense_locations, dense_half_extents};
    auto const dense_cell{dense_fixture.grid.to_cell_coord(dense_location)};
    checks.are_equal(dense_entity_count,
                     dense_fixture.grid.get_cell_entities(dense_cell).Num(),
                     TEXT("Dense cell retains every entity"));

    TArray<FExpectedTrace> const dense_cases{
        {TEXT("Trace resolves dense cell"),
         {20.f, 25.f, 25.f},
         {30.f, 25.f, 25.f},
         1,
         {24.f, 25.f, 25.f}},
    };
    check_traces(checks, dense_fixture, dense_cases);

    FVector3f const wide_half_extents{160.f, 160.f, 160.f};
    TArray<FVector3f> const wide_locations{FVector3f::ZeroVector};
    FTraceFixture const wide_fixture{wide_locations, wide_half_extents};
    auto const [min_coord, max_coord]{
        wide_fixture.grid.to_cell_coord_bounds(-wide_half_extents, wide_half_extents)};
    int32 membership_count{};
    for (int32 x{min_coord.X}; x <= max_coord.X; ++x) {
        for (int32 y{min_coord.Y}; y <= max_coord.Y; ++y) {
            for (int32 z{min_coord.Z}; z <= max_coord.Z; ++z) {
                membership_count += count_handle(wide_fixture.grid.get_cell_entities({x, y, z}),
                                                 wide_fixture.handles[0]);
            }
        }
    }
    checks.are_equal(64, membership_count, TEXT("Three-axis AABB occupies every covered cell"));

    TArray<FExpectedTrace> const wide_cases{
        {TEXT("Diagonal trace resolves three-axis multi-cell AABB"),
         {-300.f, -300.f, -300.f},
         {300.f, 300.f, 300.f},
         1,
         {-160.f, -160.f, -160.f}},
    };
    check_traces(checks, wide_fixture, wide_cases);
}

void FCollisionUniformGridTraceScenario::test_production_scale() {
    constexpr float grid_boundary{1000000.f};
    auto const inside_positive_grid_boundary{std::nextafter(grid_boundary, 0.f)};
    FVector3f const aabb_half_extents{1000.f, 1000.f, 1000.f};
    FVector3f const negative_entity{-999000.f, 0.f, 0.f};
    FVector3f const positive_entity{999000.f, 0.f, 0.f};
    TArray<FVector3f> const entity_locations{negative_entity, positive_entity};
    FTraceFixture const fixture{
        entity_locations, aabb_half_extents, FVector3f::ZeroVector, grid_dims, cell_dims};
    TArray<FExpectedTrace> const cases{
        {TEXT("Trace starts on negative production grid boundary"),
         {-grid_boundary, 0.f, 0.f},
         {-990000.f, 0.f, 0.f},
         1,
         {-grid_boundary, 0.f, 0.f},
         0},
        {TEXT("Trace ends on positive production grid boundary"),
         {990000.f, 0.f, 0.f},
         {grid_boundary, 0.f, 0.f},
         1,
         {998000.f, 0.f, 0.f},
         1},
        {TEXT("Trace follows negative production grid boundary"),
         {-grid_boundary, -5000.f, 0.f},
         {-grid_boundary, 5000.f, 0.f},
         1,
         {-grid_boundary, -1000.f, 0.f},
         0},
        {TEXT("Trace follows positive production grid boundary"),
         {grid_boundary, -5000.f, 0.f},
         {grid_boundary, 5000.f, 0.f},
         0},
        {TEXT("Trace follows one float inside positive production grid boundary"),
         {inside_positive_grid_boundary, -5000.f, 0.f},
         {inside_positive_grid_boundary, 5000.f, 0.f},
         1,
         {inside_positive_grid_boundary, -1000.f, 0.f},
         1},
        {TEXT("Outside trace only touches positive production grid boundary"),
         {grid_boundary + 10000.f, 0.f, 0.f},
         {grid_boundary, 0.f, 0.f},
         0},
    };
    check_traces(checks, fixture, cases);
}

void FCollisionUniformGridTraceScenario::test_static_geometry() {
    FVector3f const dynamic_location{100.f, 0.f, 0.f};
    FVector3f const half_extents{10.f, 10.f, 10.f};
    TArray<FVector3f> const dynamic_locations{dynamic_location};
    FTraceFixture fixture{dynamic_locations, half_extents};

    auto set_static_aabb{[&fixture](FVector3f const min_point, FVector3f const max_point) {
        WorldAABBs static_aabbs;
        static_aabbs.mins.add(min_point);
        static_aabbs.maxes.add(max_point);
        fixture.grid.set_static_aabbs(MoveTemp(static_aabbs));
    }};
    TArray<FVector3f> const starts{{-200.f, 0.f, 0.f}};
    TArray<FVector3f> const ends{{200.f, 0.f, 0.f}};

    set_static_aabb({-60.f, -10.f, -10.f}, {-40.f, 10.f, 10.f});
    auto static_hits{run_traces(fixture, starts, ends)};
    checks.are_equal(uint8{1}, static_hits.hits[0], TEXT("Static AABB is traceable"));
    checks.is_true(!static_hits.entities[0].is_valid(), TEXT("Static hit has no dynamic entity"));
    checks.are_equal(0,
                     static_hits.static_geometry_indices[0],
                     TEXT("Static hit identifies canonical static geometry"));
    checks.dist_zero(FVector3f{-60.f, 0.f, 0.f},
                     static_hits.locations[0],
                     hit_location_tolerance,
                     TEXT("Static hit reports nearest entry point"));

    fixture.grid.rebuild_grid(fixture.aabbs);
    auto const rebuilt_static_hits{run_traces(fixture, starts, ends)};
    checks.are_equal(0,
                     rebuilt_static_hits.static_geometry_indices[0],
                     TEXT("Static geometry survives dynamic rebuild"));

    set_static_aabb({140.f, -10.f, -10.f}, {160.f, 10.f, 10.f});
    auto const dynamic_hits{run_traces(fixture, starts, ends)};
    checks.are_equal(fixture.handles[0],
                     dynamic_hits.entities[0],
                     TEXT("Closer dynamic geometry wins over static geometry"));
    checks.are_equal(int32{INDEX_NONE},
                     dynamic_hits.static_geometry_indices[0],
                     TEXT("Dynamic hit clears static identity"));

    TArray<FRegistryEntityHandle> const ignored_entities{fixture.handles[0]};
    auto const ignored_dynamic_hits{run_traces(fixture, starts, ends, ignored_entities)};
    checks.is_true(!ignored_dynamic_hits.entities[0].is_valid(),
                   TEXT("Ignored dynamic entity is not returned"));
    checks.are_equal(0,
                     ignored_dynamic_hits.static_geometry_indices[0],
                     TEXT("Static geometry behind ignored entity remains traceable"));

    set_static_aabb({90.f, -10.f, -10.f}, {110.f, 10.f, 10.f});
    auto const tied_hits{run_traces(fixture, starts, ends)};
    checks.are_equal(fixture.handles[0],
                     tied_hits.entities[0],
                     TEXT("Dynamic geometry wins exact-distance static tie"));
}

void FCollisionUniformGridTraceScenario::test_static_harvesting() {
    auto* const harvested_actor{context_.world.SpawnActor<ASandboxTestDerivedCollisionActor>(
        ASandboxTestDerivedCollisionActor::StaticClass(), FTransform{FVector::ZeroVector})};
    auto* const omitted_actor{context_.world.SpawnActor<ASandboxTestOmittedCollisionActor>(
        ASandboxTestOmittedCollisionActor::StaticClass(), FTransform{FVector{300.f, 0.f, 0.f}})};
    if (!checks.is_valid(harvested_actor, TEXT("Harvest test actor is spawned")) ||
        !checks.is_valid(omitted_actor, TEXT("Omitted test actor is spawned"))) {
        return;
    }

    auto* const unsupported_component{NewObject<UInstancedStaticMeshComponent>(harvested_actor)};
    unsupported_component->SetMobility(EComponentMobility::Static);
    unsupported_component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    harvested_actor->AddInstanceComponent(unsupported_component);
    unsupported_component->RegisterComponent();

    FCollisionGridConfig config;
    config.grid_size = FVector3f{4000.f, 4000.f, 4000.f};
    config.cell_size = FVector3f{100.f, 100.f, 100.f};
    config.harvested_collision_actor_classes.Add(ASandboxTestCollisionActor::StaticClass());
    config.omitted_collision_actor_classes.Add(ASandboxTestOmittedCollisionActor::StaticClass());

    FTestEntityRegistry registry;
    ioj::FCollisionSystem collision;
    collision.set_entity_registry(registry);
    auto& grid{collision.get_uniform_grid()};
    grid.set_grid_dims(config.calculate_grid_dimensions());
    grid.set_cell_dims(config.cell_size);
    collision.initialise_static_geometry(context_.world, config);

    auto const sources{collision.get_static_collision_sources()};
    auto const source_index{sources.IndexOfByPredicate([harvested_actor](auto const& source) {
        return source.actor.Get() == harvested_actor &&
               source.component.Get() == harvested_actor->get_collision_component();
    })};
    int32 harvested_actor_source_count{};
    for (auto const& source : sources) {
        if (source.actor.Get() == harvested_actor) {
            ++harvested_actor_source_count;
        }
    }
    checks.is_true(source_index != INDEX_NONE,
                   TEXT("Configured base class harvests subclass actor"));
    checks.are_equal(1,
                     harvested_actor_source_count,
                     TEXT("Unsupported component is not added to static geometry"));
    checks.is_true(harvested_actor->get_collision_component()->GetCollisionEnabled() ==
                       ECollisionEnabled::NoCollision,
                   TEXT("Successful harvest disables Unreal collision"));
    checks.is_true(omitted_actor->get_collision_component()->GetCollisionEnabled() ==
                       ECollisionEnabled::QueryOnly,
                   TEXT("Omitted actor keeps Unreal collision"));
    checks.is_true(unsupported_component->GetCollisionEnabled() == ECollisionEnabled::QueryOnly,
                   TEXT("Failed harvest leaves Unreal collision enabled"));

    auto const source_count{sources.Num()};
    collision.initialise_static_geometry(context_.world, config);
    checks.are_equal(source_count,
                     collision.get_static_collision_sources().Num(),
                     TEXT("Reinitialization restores and reharvests static geometry"));
    checks.is_true(harvested_actor->get_collision_component()->GetCollisionEnabled() ==
                       ECollisionEnabled::NoCollision,
                   TEXT("Reharvested component remains owned by custom collision"));
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
            case ECollisionUniformGridTraceScenario::VariedGridGeometry:
                test_varied_grid_geometry();
                break;
            case ECollisionUniformGridTraceScenario::BoundaryPrecision:
                test_boundary_precision();
                break;
            case ECollisionUniformGridTraceScenario::RebuildLifecycle:
                test_rebuild_lifecycle();
                break;
            case ECollisionUniformGridTraceScenario::DeterministicReferenceSweep:
                test_deterministic_reference_sweep();
                break;
            case ECollisionUniformGridTraceScenario::InvarianceProperties:
                test_invariance_properties();
                break;
            case ECollisionUniformGridTraceScenario::EmptyBatchesAndOutputReuse:
                test_empty_batches_and_output_reuse();
                break;
            case ECollisionUniformGridTraceScenario::DenseAndWideAABBs:
                test_dense_and_wide_aabbs();
                break;
            case ECollisionUniformGridTraceScenario::ProductionScale:
                test_production_scale();
                break;
            case ECollisionUniformGridTraceScenario::StaticGeometry:
                test_static_geometry();
                break;
            case ECollisionUniformGridTraceScenario::StaticHarvesting:
                test_static_harvesting();
                break;
        }

        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    });
}
}
