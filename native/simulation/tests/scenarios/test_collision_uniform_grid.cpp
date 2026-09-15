#include "test_collision_uniform_grid.h"
#include <bit>
#include "../support/simulation_test_support.h"

#include <ioj/sim/world_aabb_operations.h>

#include <ioj/sim/capital_ships/sim.h>
#include <ioj/sim/collision/collision_system.h>
#include <ioj/sim/collision/collision_uniform_grid.h>
#include <ioj/sim/entity_registry.h>
#include <ioj/sim/fighters/sim.h>
#include <ioj/sim/spatial_query_manager.h>
#include <ioj/sim/trace_hits.h>

namespace ioj::sim {
namespace {
collision::CellCoord const grid_dims{400, 400, 5};
Vector3f const cell_dims{{5000.f, 5000.f, 20000.f}};
collision::CellCoord const trace_grid_dims{8, 8, 8};
Vector3f const trace_cell_dims{{100.f, 100.f, 100.f}};
constexpr float hit_location_tolerance{0.001f};

struct TraceFixture {
    TraceFixture(std::span<Vector3f const> const locations,
                 Vector3f const half_extents,
                 Vector3f const aabb_centre = Vector3f{},
                 collision::CellCoord const fixture_grid_dims = trace_grid_dims,
                 Vector3f const fixture_cell_dims = trace_cell_dims,
                 std::span<EntityType const> const fixture_entity_types = {},
                 Rotator3f const rotation = Rotator3f{}) {
        auto const count{static_cast<std::int32_t>(locations.size())};
        assert(fixture_entity_types.empty() ||
               static_cast<std::int32_t>(fixture_entity_types.size()) == count);
        RegistryEntityData entity_data;
        entity_data.add_defaulted(count);
        for (std::int32_t i{}; i < count; ++i) {
            entity_data.locations.set(i, locations[i]);
            entity_data.rotations.set(i, rotation);
            entity_data.healths[i] = 1;
            entity_data.teams[i] = Team::Blue;
            entity_data.entity_types[i] =
                fixture_entity_types.empty() ? EntityType::CapitalShip : fixture_entity_types[i];
            entity_data.alive[i] = 1;
        }
        auto const spawned{registry.add_entities(entity_data.get_const_view())};
        handles = tests::copy_handles(spawned.registry_handles);

        set_entity_aabb(EntityType::CapitalShip, aabb_centre, half_extents);

        grid.set_grid_dims({fixture_grid_dims.x, fixture_grid_dims.y, fixture_grid_dims.z});
        grid.set_cell_dims(fixture_cell_dims);
        grid.rebuild_grid(aabbs);
    }

    void set_entity_aabb(EntityType const entity_type,
                         Vector3f const centre,
                         Vector3f const half_extents) {
        auto const aabb_index{std::to_underlying(entity_type)};
        aabbs.centre_xs[aabb_index] = centre.X;
        aabbs.centre_ys[aabb_index] = centre.Y;
        aabbs.centre_zs[aabb_index] = centre.Z;
        aabbs.half_extent_xs[aabb_index] = half_extents.X;
        aabbs.half_extent_ys[aabb_index] = half_extents.Y;
        aabbs.half_extent_zs[aabb_index] = half_extents.Z;
    }

    void update_entities(std::span<Vector3f const> const locations,
                         std::span<std::uint8_t const> const alive) {
        auto const count{static_cast<std::int32_t>(handles.size())};
        assert(static_cast<std::int32_t>(locations.size()) == count);
        assert(static_cast<std::int32_t>(alive.size()) == count);

        RegistryEntityData entity_data;
        entity_data.add_defaulted(count);
        for (std::int32_t i{}; i < count; ++i) {
            entity_data.locations.set(i, locations[i]);
            entity_data.healths[i] = 1;
            entity_data.teams[i] = Team::Blue;
            entity_data.entity_types[i] = EntityType::CapitalShip;
            entity_data.alive[i] = alive[i];
        }
        EntityRegistry::ConstView const updates{
            .indices = {handles.data(), static_cast<std::size_t>(count)},
            .data = entity_data.get_const_view(),
        };
        EntityDeathInfo death_info;
        for (std::int32_t i{}; i < count; ++i) {
            if (alive[i] == 0 && registry.get_alive(handles[i])) {
                death_info.add(DeathReason::Unknown, handles[i], {});
            }
        }
        registry.queue_entity_updates(updates, death_info);
        registry.commit_updates();
        registry.end_tick();
        grid.rebuild_grid(aabbs);
    }

    auto add_entity(Vector3f const location) -> RegistryEntityHandle {
        RegistryEntityData entity_data;
        entity_data.add_defaulted(1);
        entity_data.locations.set(0, location);
        entity_data.healths[0] = 1;
        entity_data.teams[0] = Team::Blue;
        entity_data.entity_types[0] = EntityType::CapitalShip;
        entity_data.alive[0] = 1;

        auto const spawned{registry.add_entities(entity_data.get_const_view())};
        grid.rebuild_grid(aabbs);
        return spawned.get_handle(0);
    }

    EntityRegistry registry;
    collision::CollisionUniformGrid grid{registry};
    std::vector<RegistryEntityHandle> handles{};
    collision::EntityAABBs aabbs;
};

auto reference_trace_aabb(Vector3f const start,
                          Vector3f const end,
                          Vector3f const aabb_min,
                          Vector3f const aabb_max) -> float {
    constexpr auto no_hit{std::numeric_limits<float>::infinity()};
    auto const delta{end - start};
    float t_min{};
    float t_max{1.f};

    for (std::int32_t axis{}; axis < 3; ++axis) {
        auto const axis_delta{delta.Elements[axis]};
        if (axis_delta == 0.f) {
            if (start.Elements[axis] < aabb_min.Elements[axis] ||
                start.Elements[axis] > aabb_max.Elements[axis]) {
                return no_hit;
            }
            continue;
        }

        auto t1{(aabb_min.Elements[axis] - start.Elements[axis]) / axis_delta};
        auto t2{(aabb_max.Elements[axis] - start.Elements[axis]) / axis_delta};
        if (t1 > t2) {
            std::swap(t1, t2);
        }

        t_min = std::max(t_min, t1);
        t_max = std::min(t_max, t2);
        if (t_min > t_max) {
            return no_hit;
        }
    }

    return t_min;
}

auto make_line_traces(std::span<Vector3f const> const starts, std::span<Vector3f const> const ends)
    -> LineTraces {
    assert(static_cast<std::int32_t>(starts.size()) == static_cast<std::int32_t>(ends.size()));

    LineTraces traces;
    auto const count{static_cast<std::int32_t>(starts.size())};
    traces.starts.reserve(count);
    traces.ends.reserve(count);
    for (std::int32_t i{}; i < count; ++i) {
        traces.starts.add(starts[i]);
        traces.ends.add(ends[i]);
    }
    return traces;
}

auto run_traces(TraceFixture const& fixture,
                std::span<Vector3f const> const starts,
                std::span<Vector3f const> const ends,
                std::span<RegistryEntityHandle const> const ignored_entities = {}) -> TraceHits {
    auto const traces{make_line_traces(starts, ends)};

    TraceHits hits;
    hits.add_defaulted(traces.num());
    if (ignored_entities.empty()) {
        fixture.grid.trace_aabbs(traces.get_const_view(), hits.get_view());
    } else {
        fixture.grid.trace_aabbs(
            traces.get_const_view(),
            hits.get_view(),
            {ignored_entities.data(),
             static_cast<std::size_t>(static_cast<std::int32_t>(ignored_entities.size()))});
    }
    return hits;
}

auto run_sweeps(TraceFixture const& fixture,
                std::span<Vector3f const> const starts,
                std::span<Vector3f const> const ends,
                Vector3f const moving_half_extent,
                std::span<RegistryEntityHandle const> const ignored_entities = {},
                collision::TraceEntityFilter const entity_filter =
                    collision::TraceEntityFilter::None) -> TraceHits {
    auto const traces{make_line_traces(starts, ends)};

    TraceHits hits;
    hits.add_defaulted(traces.num());
    fixture.grid.sweep_aabbs(
        traces.get_const_view(),
        moving_half_extent,
        hits.get_view(),
        {ignored_entities.data(),
         static_cast<std::size_t>(static_cast<std::int32_t>(ignored_entities.size()))},
        entity_filter);
    return hits;
}

struct ExpectedTrace {
    char const* name;
    Vector3f start;
    Vector3f end;
    std::uint8_t expected_hit;
    Vector3f expected_location{};
    std::int32_t expected_entity_index{};
};

void check_traces(TraceFixture const& fixture, std::span<ExpectedTrace const> const cases) {
    std::vector<Vector3f> starts{};
    std::vector<Vector3f> ends{};
    starts.reserve(static_cast<std::int32_t>(cases.size()));
    ends.reserve(static_cast<std::int32_t>(cases.size()));

    for (auto const& trace_case : cases) {
        starts.push_back(trace_case.start);
        ends.push_back(trace_case.end);
    }

    auto const hits{run_traces(fixture, starts, ends)};
    auto const count{static_cast<std::int32_t>(cases.size())};
    for (std::int32_t i{}; i < count; ++i) {
        auto const& trace_case{cases[i]};
        std::string const hit_description{" has expected hit flag" +
                                          ::testing::PrintToString(trace_case.name)};
        tests::expect_equal(trace_case.expected_hit, hits.hits[i], hit_description);
        if (trace_case.expected_hit == 0 || hits.hits[i] == 0) {
            continue;
        }

        std::string const entity_description{" resolves expected entity" +
                                             ::testing::PrintToString(trace_case.name)};
        tests::expect_equal(fixture.handles[trace_case.expected_entity_index],
                            hits.entities[i],
                            entity_description);

        std::string const location_description{" resolves expected hit location" +
                                               ::testing::PrintToString(trace_case.name)};
        tests::expect_distance_near(trace_case.expected_location,
                                    hits.locations[i],
                                    hit_location_tolerance,
                                    location_description);
    }
}

auto count_handle(std::span<RegistryEntityHandle const> const handles,
                  RegistryEntityHandle const expected) -> std::int32_t {
    std::int32_t count{};
    for (auto const handle : handles) {
        if (handle == expected) {
            ++count;
        }
    }
    return count;
}
}

void run_worldless_collision_uniform_grid_membership(tests::SimulationFixture const& config) {
    auto data{tests::make_simulation_data(config)};
    auto const player_index{tests::add_player_spawn(
        data,
        tests::make_player_spawn(config,
                                 Transform3d{.rotation = ml::Quaternion4d{},
                                             .location = ml::Vector3d{-1500.f, -1500.f, 0.f}}))};
    tests::add_capital_spawn(data,
                             Vector3f{{-500.f, -500.f, 0.f}},
                             Team::Blue,
                             player_index,
                             0.f,
                             data.capital_ships.spawn_delay);
    tests::add_turret_spawn(data, HMM_V3(500.f, 500.f, 0.f), {}, Team::Blue);
    tests::add_spinner_spawn(data, HMM_V3(1500.f, 1500.f, 0.f), 0.f, 0);

    tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    harness.timeline.finish_at(0.1);
    tests::expect_true(harness.run_until_timeline_finished(1.0),
                       "Collision-grid timeline completes");

    auto& simulation{harness.get_simulation()};
    auto const* const player{simulation.get_player_ship_simulation()};
    auto const fighter_handles{simulation.get_fighters().get_handles()};
    tests::expect_not_null(player, "Collision-grid player ship is available");
    tests::expect_true(!fighter_handles.empty(), "Collision-grid fighter is placed");
    if (::testing::Test::HasFailure()) {
        return;
    }

    std::array<RegistryEntityHandle, 5> const expected_handles{
        player->registry_handle,
        simulation.get_capital_ships().get_handle(0),
        fighter_handles[0],
        simulation.get_turrets().get_read_view().entities.handles[0],
        simulation.get_spinners().get_read_view().entities.handles[0],
    };
    auto const& registry{harness.get_registry()};
    auto const& collision{simulation.get_spatial_query_manager().get_collision_system()};
    auto const& grid{collision.get_uniform_grid()};
    auto const& entity_aabbs{collision.get_entity_aabbs()};
    tests::expect_true(grid.get_grid_dims() ==
                           collision::CellCoord{grid_dims.x, grid_dims.y, grid_dims.z},
                       "Collision grid uses the production dimensions");
    auto const cell_dimensions_match{grid.get_cell_dims() == cell_dims};
    tests::expect_true(cell_dimensions_match != 0, "Collision grid uses the production cell size");

    auto const handle_count{static_cast<std::int32_t>(expected_handles.size())};
    for (std::int32_t i{}; i < handle_count; ++i) {
        auto const handle{expected_handles[i]};
        auto const entity_type{static_cast<EntityType>(i)};
        if (!tests::expect_true(registry.is_valid_alive(handle),
                                "Expected collision-grid  entity is alive" +
                                    ::testing::PrintToString(entity_type))) {
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
        if (!tests::expect_true(grid.is_cell_coord_in_bounds(min_coord, max_coord),
                                "Expected collision-grid  entity is placed" +
                                    ::testing::PrintToString(entity_type))) {
            continue;
        }

        std::int32_t expected_cell_count{};
        std::int32_t found_cell_count{};
        for (std::int32_t x{min_coord.x}; x <= max_coord.x; ++x) {
            for (std::int32_t y{min_coord.y}; y <= max_coord.y; ++y) {
                for (std::int32_t z{min_coord.z}; z <= max_coord.z; ++z) {
                    ++expected_cell_count;
                    found_cell_count += count_handle(grid.get_cell_entities({x, y, z}), handle);
                }
            }
        }
        tests::expect_equal(expected_cell_count,
                            found_cell_count,
                            "Expected entity has the expected collision-grid membership",
                            i);
    }
}

/* ------------------------------------------------------------------------------------------ */
// Trace scenarios
/* ------------------------------------------------------------------------------------------ */
class CollisionUniformGridTraceRunner final {
  public:
    CollisionUniformGridTraceRunner(CollisionUniformGridTraceScenario scenario);
    void run();
  private:
    void test_hits_and_misses();
    void test_stops_at_endpoint();
    void test_returns_nearest_hit();
    void test_handles_zero_length_traces();
    void test_includes_negative_endpoint_boundary();
    void test_applies_aabb_centre();
    void test_axis_parallel_and_origin();
    void test_surface_contacts();
    void test_grid_boundary_traversal();
    void test_short_and_near_parallel_segments();
    void test_clips_to_grid_bounds();
    void test_degenerate_aabbs();
    void test_cross_cell_nearest_hit();
    void test_varied_grid_geometry();
    void test_boundary_precision();
    void test_rebuild_lifecycle();
    void test_deterministic_reference_sweep();
    void test_invariance_properties();
    void test_empty_batches_and_output_reuse();
    void test_dense_and_wide_aabbs();
    void test_production_scale();
    void test_static_geometry();
    CollisionUniformGridTraceScenario scenario_;
};

CollisionUniformGridTraceRunner::CollisionUniformGridTraceRunner(
    CollisionUniformGridTraceScenario const scenario)
    : scenario_{scenario} {}

void CollisionUniformGridTraceRunner::test_hits_and_misses() {
    Vector3f const entity_centre{};
    Vector3f const aabb_half_extents{{10.f, 10.f, 10.f}};
    Vector3f const left_of_aabb{{-150.f, 0.f, 0.f}};
    Vector3f const right_of_aabb{{150.f, 0.f, 0.f}};
    Vector3f const negative_diagonal_start{{-150.f, -150.f, -150.f}};
    Vector3f const positive_diagonal_end{{150.f, 150.f, 150.f}};
    Vector3f const off_axis_start{{-150.f, 25.f, 0.f}};
    Vector3f const off_axis_end{{150.f, 25.f, 0.f}};
    Vector3f const left_face_contact{{-10.f, 0.f, 0.f}};
    Vector3f const right_face_contact{{10.f, 0.f, 0.f}};
    Vector3f const negative_corner_contact{{-10.f, -10.f, -10.f}};
    Vector3f const unused_miss_location{};

    std::vector<Vector3f> const entity_locations{entity_centre};
    TraceFixture const fixture{entity_locations, aabb_half_extents};

    std::vector<Vector3f> const starts{
        left_of_aabb,
        right_of_aabb,
        negative_diagonal_start,
        off_axis_start,
        entity_centre,
    };
    std::vector<Vector3f> const ends{
        right_of_aabb,
        left_of_aabb,
        positive_diagonal_end,
        off_axis_end,
        right_of_aabb,
    };
    auto const hits{run_traces(fixture, starts, ends)};

    std::array<std::uint8_t, 5> const expected_hit_flags{1, 1, 1, 0, 1};
    std::array<Vector3f, 5> const expected_locations{
        left_face_contact,
        right_face_contact,
        negative_corner_contact,
        unused_miss_location,
        entity_centre,
    };

    auto const count{static_cast<std::int32_t>(expected_hit_flags.size())};
    for (std::int32_t i{}; i < count; ++i) {
        tests::expect_equal(expected_hit_flags[i], hits.hits[i], "Trace has expected hit flag", i);
        if (expected_hit_flags[i] == 0) {
            continue;
        }

        tests::expect_equal(
            fixture.handles[0], hits.entities[i], "Trace resolves expected entity", i);
        tests::expect_distance_near(expected_locations[i],
                                    hits.locations[i],
                                    hit_location_tolerance,
                                    "Trace resolves expected hit location",
                                    i);
    }
}

void CollisionUniformGridTraceRunner::test_stops_at_endpoint() {
    Vector3f const entity_centre{{50.f, 0.f, 0.f}};
    Vector3f const aabb_half_extents{{10.f, 10.f, 10.f}};
    Vector3f const trace_start{};
    Vector3f const trace_endpoint_before_aabb{{20.f, 0.f, 0.f}};

    std::vector<Vector3f> const entity_locations{entity_centre};
    TraceFixture const fixture{entity_locations, aabb_half_extents};
    std::vector<Vector3f> const starts{trace_start};
    std::vector<Vector3f> const ends{trace_endpoint_before_aabb};

    auto const hits{run_traces(fixture, starts, ends)};

    tests::expect_equal(std::uint8_t{0}, hits.hits[0], "AABB beyond trace endpoint is not hit");
}

void CollisionUniformGridTraceRunner::test_returns_nearest_hit() {
    Vector3f const far_entity_centre{{60.f, 0.f, 0.f}};
    Vector3f const near_entity_centre{{20.f, 0.f, 0.f}};
    Vector3f const aabb_half_extents{{5.f, 5.f, 5.f}};
    Vector3f const trace_start{};
    Vector3f const trace_end{{90.f, 0.f, 0.f}};
    Vector3f const expected_near_contact{{15.f, 0.f, 0.f}};

    std::vector<Vector3f> const entity_locations{
        far_entity_centre,
        near_entity_centre,
    };
    TraceFixture const fixture{entity_locations, aabb_half_extents};
    std::vector<Vector3f> const starts{trace_start};
    std::vector<Vector3f> const ends{trace_end};

    auto const hits{run_traces(fixture, starts, ends)};

    tests::expect_equal(std::uint8_t{1}, hits.hits[0], "Trace through two AABBs records a hit");
    tests::expect_equal(
        fixture.handles[1], hits.entities[0], "Trace returns nearest intersecting entity");
    tests::expect_distance_near(expected_near_contact,
                                hits.locations[0],
                                hit_location_tolerance,
                                "Trace returns nearest intersection location");
}

void CollisionUniformGridTraceRunner::test_handles_zero_length_traces() {
    Vector3f const entity_centre{{50.f, 0.f, 0.f}};
    Vector3f const aabb_half_extents{{10.f, 10.f, 10.f}};
    Vector3f const point_outside_aabb{};

    std::vector<Vector3f> const entity_locations{entity_centre};
    TraceFixture const fixture{entity_locations, aabb_half_extents};
    std::vector<Vector3f> const starts{
        entity_centre,
        point_outside_aabb,
    };

    auto const hits{run_traces(fixture, starts, starts)};

    tests::expect_equal(
        std::uint8_t{1}, hits.hits[0], "Stationary point inside AABB records a hit");
    tests::expect_equal(
        fixture.handles[0], hits.entities[0], "Stationary point resolves containing entity");
    tests::expect_distance_near(starts[0],
                                hits.locations[0],
                                hit_location_tolerance,
                                "Stationary point hit location is the trace point");
    tests::expect_equal(
        std::uint8_t{0}, hits.hits[1], "Stationary point outside AABB does not record a hit");
}

void CollisionUniformGridTraceRunner::test_includes_negative_endpoint_boundary() {
    Vector3f const entity_centre{{-10.f, 0.f, 0.f}};
    Vector3f const aabb_half_extents{{10.f, 10.f, 10.f}};
    Vector3f const trace_start{{10.f, 0.f, 0.f}};
    Vector3f const boundary_contact{};

    std::vector<Vector3f> const entity_locations{entity_centre};
    TraceFixture const fixture{entity_locations, aabb_half_extents};
    std::vector<Vector3f> const starts{trace_start};
    std::vector<Vector3f> const ends{boundary_contact};

    auto const hits{run_traces(fixture, starts, ends)};

    tests::expect_equal(std::uint8_t{1}, hits.hits[0], "Trace includes AABB touched at endpoint");
    if (hits.hits[0] == 0) {
        return;
    }

    tests::expect_equal(
        fixture.handles[0], hits.entities[0], "Endpoint trace resolves touched entity");
    tests::expect_distance_near(boundary_contact,
                                hits.locations[0],
                                hit_location_tolerance,
                                "Endpoint trace returns boundary contact location");
}

void CollisionUniformGridTraceRunner::test_applies_aabb_centre() {
    // A quarter turn moves the offset centre into a different grid cell and swaps X/Y extents.
    TraceFixture rotated{std::vector<Vector3f>{Vector3f{}},
                         Vector3f{{20.f, 5.f, 10.f}},
                         Vector3f{{150.f, 0.f, 0.f}},
                         trace_grid_dims,
                         trace_cell_dims,
                         {},
                         Rotator3f{0.f, 90.f, 0.f}};
    std::vector<Vector3f> const rotated_starts{{{-50.f, 150.f, 0.f}}, {{100.f, 0.f, 0.f}}};
    std::vector<Vector3f> const rotated_ends{{{50.f, 150.f, 0.f}}, {{200.f, 0.f, 0.f}}};
    auto const rotated_hits{run_traces(rotated, rotated_starts, rotated_ends)};
    tests::expect_equal(
        std::uint8_t{1}, rotated_hits.hits[0], "Trace finds rotated box in its new grid cell");
    tests::expect_equal(std::uint8_t{0}, rotated_hits.hits[1], "Trace misses old unrotated box");
    tests::expect_distance_near(Vector3f{{-5.f, 150.f, 0.f}},
                                rotated_hits.locations[0],
                                0.001f,
                                "Rotated box has swapped extents");
    auto const swept{run_sweeps(rotated, rotated_starts, rotated_ends, Vector3f{{2.f, 2.f, 2.f}})};
    tests::expect_equal(std::uint8_t{1}, swept.hits[0], "Sweep uses rotated cached box");
    tests::expect_distance_near(Vector3f{{-7.f, 150.f, 0.f}},
                                swept.locations[0],
                                0.001f,
                                "Sweep expands rotated world bounds");
    auto const cached{rotated.grid.get_entity_world_bounds()};
    tests::expect_distance_near(Vector3f{{-5.f, 130.f, -10.f}},
                                collision::min_at(cached, 0),
                                0.001f,
                                "Visualisation reads the same rotated cached bounds");

    rotated.update_entities(std::vector<Vector3f>{Vector3f{}}, std::vector<std::uint8_t>{1});
    auto const updated_hits{run_traces(rotated, rotated_starts, rotated_ends)};
    tests::expect_equal(std::uint8_t{0},
                        updated_hits.hits[0],
                        "Registry rotation update removes old rotated bounds");
    tests::expect_equal(
        std::uint8_t{1}, updated_hits.hits[1], "Registry rotation update reaches grid queries");
    rotated.update_entities(std::vector<Vector3f>{Vector3f{}}, std::vector<std::uint8_t>{0});
    auto const reused_handle{rotated.add_entity(Vector3f{})};
    tests::expect_equal(
        rotated.handles[0].index, reused_handle.index, "Fixture reuses registry slot");
    auto const reused_hits{run_traces(rotated, rotated_starts, rotated_ends)};
    tests::expect_equal(
        reused_handle, reused_hits.entities[1], "Reused slot has current bounds and generation");

    Vector3f const entity_location{};
    Vector3f const local_aabb_centre{{40.f, 0.f, 0.f}};
    Vector3f const aabb_half_extents{{10.f, 10.f, 10.f}};
    Vector3f const trace_start{{20.f, 0.f, 0.f}};
    Vector3f const trace_end{{60.f, 0.f, 0.f}};
    Vector3f const expected_contact{{30.f, 0.f, 0.f}};

    std::vector<Vector3f> const entity_locations{entity_location};
    TraceFixture const fixture{entity_locations, aabb_half_extents, local_aabb_centre};
    std::vector<Vector3f> const starts{trace_start};
    std::vector<Vector3f> const ends{trace_end};

    auto const hits{run_traces(fixture, starts, ends)};

    tests::expect_equal(std::uint8_t{1}, hits.hits[0], "Trace hits locally centred AABB");
    if (hits.hits[0] == 0) {
        return;
    }

    tests::expect_equal(
        fixture.handles[0], hits.entities[0], "Trace resolves locally centred entity");
    tests::expect_distance_near(expected_contact,
                                hits.locations[0],
                                hit_location_tolerance,
                                "Trace applies local AABB centre to hit location");

    std::vector<EntityType> const entity_types{
        EntityType::PlayerShip,
        EntityType::Turret,
        EntityType::CapitalShip,
        EntityType::Fighter,
        EntityType::TubeSpinner,
    };
    std::vector<Vector3f> const mixed_locations{
        {{-300.f, 0.f, 0.f}},
        {{-150.f, 0.f, 0.f}},
        {{0.f, 0.f, 0.f}},
        {{150.f, 0.f, 0.f}},
        {{300.f, 0.f, 0.f}},
    };
    std::vector<Vector3f> const local_centres{
        {{5.f, -20.f, 3.f}},
        {{-6.f, -10.f, -2.f}},
        {{0.f, 0.f, 0.f}},
        {{8.f, 10.f, -4.f}},
        {{-9.f, 20.f, 5.f}},
    };
    std::vector<Vector3f> const half_extents{
        {{4.f, 5.f, 6.f}},
        {{7.f, 8.f, 9.f}},
        {{10.f, 11.f, 12.f}},
        {{13.f, 14.f, 15.f}},
        {{16.f, 17.f, 18.f}},
    };
    TraceFixture mixed_fixture{mixed_locations,
                               half_extents[collision::EntityAABBs::capital_ship_index],
                               local_centres[collision::EntityAABBs::capital_ship_index],
                               trace_grid_dims,
                               trace_cell_dims,
                               entity_types};
    auto const entity_type_count{static_cast<std::int32_t>(entity_types.size())};
    for (std::int32_t i{}; i < entity_type_count; ++i) {
        mixed_fixture.set_entity_aabb(entity_types[i], local_centres[i], half_extents[i]);
    }
    mixed_fixture.grid.rebuild_grid(mixed_fixture.aabbs);

    std::vector<Vector3f> mixed_starts{};
    std::vector<Vector3f> mixed_ends{};
    mixed_starts.reserve(entity_type_count);
    mixed_ends.reserve(entity_type_count);
    for (std::int32_t i{}; i < entity_type_count; ++i) {
        auto const world_centre{mixed_locations[i] + local_centres[i]};
        mixed_starts.push_back(world_centre - Vector3f{{0.f, 50.f, 0.f}});
        mixed_ends.push_back(world_centre + Vector3f{{0.f, 50.f, 0.f}});
    }

    auto const mixed_hits{run_traces(mixed_fixture, mixed_starts, mixed_ends)};
    for (std::int32_t i{}; i < entity_type_count; ++i) {
        tests::expect_equal(
            std::uint8_t{1}, mixed_hits.hits[i], "Mixed entity-type trace records a hit", i);
        if (mixed_hits.hits[i] == 0) {
            continue;
        }

        tests::expect_equal(mixed_fixture.handles[i],
                            mixed_hits.entities[i],
                            "Mixed entity-type trace resolves its entity",
                            i);
        auto const world_centre{mixed_locations[i] + local_centres[i]};
        auto const expected_type_contact{world_centre - Vector3f{{0.f, half_extents[i].Y, 0.f}}};
        tests::expect_distance_near(expected_type_contact,
                                    mixed_hits.locations[i],
                                    hit_location_tolerance,
                                    "Mixed entity-type trace applies its AABB row",
                                    i);
    }
}

void CollisionUniformGridTraceRunner::test_axis_parallel_and_origin() {
    Vector3f const entity_centre{};
    Vector3f const aabb_half_extents{{10.f, 10.f, 10.f}};
    constexpr float trace_extent{150.f};
    constexpr float outside_slab{11.f};
    constexpr float near_parallel_offset{9.f};
    constexpr float near_parallel_delta{0.0001f};
    auto const near_parallel_entry_y{near_parallel_offset +
                                     near_parallel_delta *
                                         ((trace_extent - 10.f) / (2.f * trace_extent))};

    std::vector<Vector3f> const entity_locations{entity_centre};
    TraceFixture const fixture{entity_locations, aabb_half_extents};
    std::vector<ExpectedTrace> const cases{
        {"Positive X trace through origin",
         {{-trace_extent, 0.f, 0.f}},
         {{trace_extent, 0.f, 0.f}},
         1,
         {{-10.f, 0.f, 0.f}}},
        {"Negative X trace through origin",
         {{trace_extent, 0.f, 0.f}},
         {{-trace_extent, 0.f, 0.f}},
         1,
         {{10.f, 0.f, 0.f}}},
        {"Positive Y trace through origin",
         {{0.f, -trace_extent, 0.f}},
         {{0.f, trace_extent, 0.f}},
         1,
         {{0.f, -10.f, 0.f}}},
        {"Negative Y trace through origin",
         {{0.f, trace_extent, 0.f}},
         {{0.f, -trace_extent, 0.f}},
         1,
         {{0.f, 10.f, 0.f}}},
        {"Positive Z trace through origin",
         {{0.f, 0.f, -trace_extent}},
         {{0.f, 0.f, trace_extent}},
         1,
         {{0.f, 0.f, -10.f}}},
        {"Negative Z trace through origin",
         {{0.f, 0.f, trace_extent}},
         {{0.f, 0.f, -trace_extent}},
         1,
         {{0.f, 0.f, 10.f}}},
        {"X-parallel trace outside Y slab",
         {{-trace_extent, outside_slab, 0.f}},
         {{trace_extent, outside_slab, 0.f}},
         0},
        {"Y-parallel trace outside X slab",
         {{outside_slab, -trace_extent, 0.f}},
         {{outside_slab, trace_extent, 0.f}},
         0},
        {"Z-parallel trace outside X slab",
         {{outside_slab, 0.f, -trace_extent}},
         {{outside_slab, 0.f, trace_extent}},
         0},
        {"Signed-zero components preserve axis-parallel hit",
         {{-trace_extent, -0.0f, 0.f}},
         {{trace_extent, 0.0f, -0.0f}},
         1,
         {{-10.f, 0.f, 0.f}}},
        {"Near-parallel trace inside slab",
         {{-trace_extent, near_parallel_offset, 0.f}},
         {{trace_extent, near_parallel_offset + near_parallel_delta, 0.f}},
         1,
         {{-10.f, near_parallel_entry_y, 0.f}}},
        {"Near-parallel trace outside slab",
         {{-trace_extent, outside_slab, 0.f}},
         {{trace_extent, outside_slab + near_parallel_delta, 0.f}},
         0},
    };

    check_traces(fixture, cases);
}

void CollisionUniformGridTraceRunner::test_surface_contacts() {
    Vector3f const entity_centre{};
    Vector3f const aabb_half_extents{{10.f, 10.f, 10.f}};
    constexpr float outside_face{20.f};
    constexpr float face{10.f};
    constexpr float just_outside_face{10.5f};

    std::vector<Vector3f> const entity_locations{entity_centre};
    TraceFixture const fixture{entity_locations, aabb_half_extents};
    std::vector<ExpectedTrace> const cases{
        {"Segment ends on negative X face",
         {{-outside_face, 0.f, 0.f}},
         {{-face, 0.f, 0.f}},
         1,
         {{-face, 0.f, 0.f}}},
        {"Segment ends on positive X face",
         {{outside_face, 0.f, 0.f}},
         {{face, 0.f, 0.f}},
         1,
         {{face, 0.f, 0.f}}},
        {"Segment ends on negative Y face",
         {{0.f, -outside_face, 0.f}},
         {{0.f, -face, 0.f}},
         1,
         {{0.f, -face, 0.f}}},
        {"Segment ends on positive Y face",
         {{0.f, outside_face, 0.f}},
         {{0.f, face, 0.f}},
         1,
         {{0.f, face, 0.f}}},
        {"Segment ends on negative Z face",
         {{0.f, 0.f, -outside_face}},
         {{0.f, 0.f, -face}},
         1,
         {{0.f, 0.f, -face}}},
        {"Segment ends on positive Z face",
         {{0.f, 0.f, outside_face}},
         {{0.f, 0.f, face}},
         1,
         {{0.f, 0.f, face}}},
        {"Segment starts on face and points outward",
         {{face, 0.f, 0.f}},
         {{outside_face, 0.f, 0.f}},
         1,
         {{face, 0.f, 0.f}}},
        {"Segment starts on face and points inward",
         {{-face, 0.f, 0.f}},
         {{0.f, 0.f, 0.f}},
         1,
         {{-face, 0.f, 0.f}}},
        {"Segment lies on AABB face",
         {{-outside_face, face, 0.f}},
         {{outside_face, face, 0.f}},
         1,
         {{-face, face, 0.f}}},
        {"Segment lies on AABB edge",
         {{-outside_face, face, face}},
         {{outside_face, face, face}},
         1,
         {{-face, face, face}}},
        {"Segment grazes AABB corner",
         {{-outside_face, 0.f, 0.f}},
         {{0.f, outside_face, outside_face}},
         1,
         {{-face, face, face}}},
        {"Parallel segment just outside face",
         {{-outside_face, just_outside_face, 0.f}},
         {{outside_face, just_outside_face, 0.f}},
         0},
        {"Stationary point on face", {{face, 0.f, 0.f}}, {{face, 0.f, 0.f}}, 1, {{face, 0.f, 0.f}}},
        {"Stationary point on corner",
         {{face, face, face}},
         {{face, face, face}},
         1,
         {{face, face, face}}},
        {"Stationary point just outside face",
         {{just_outside_face, 0.f, 0.f}},
         {{just_outside_face, 0.f, 0.f}},
         0},
    };

    check_traces(fixture, cases);
}

void CollisionUniformGridTraceRunner::test_grid_boundary_traversal() {
    Vector3f const aabb_half_extents{{10.f, 10.f, 10.f}};
    constexpr float trace_extent{150.f};

    {
        std::vector<Vector3f> const entity_locations{{{-10.f, 0.f, 0.f}}};
        TraceFixture const fixture{entity_locations, aabb_half_extents};
        std::vector<ExpectedTrace> const cases{
            {"Y trace lies on X cell plane",
             {{0.f, -trace_extent, 0.f}},
             {{0.f, trace_extent, 0.f}},
             1,
             {{0.f, -10.f, 0.f}}},
            {"Trace ends at origin from positive X cell",
             {{trace_extent, 0.f, 0.f}},
             {{0.f, 0.f, 0.f}},
             1,
             {{0.f, 0.f, 0.f}}},
            {"Trace starts at origin toward negative X cell",
             {{0.f, 0.f, 0.f}},
             {{-trace_extent, 0.f, 0.f}},
             1,
             {{0.f, 0.f, 0.f}}},
        };
        check_traces(fixture, cases);
    }

    {
        std::vector<Vector3f> const entity_locations{{{0.f, -10.f, 0.f}}};
        TraceFixture const fixture{entity_locations, aabb_half_extents};
        std::vector<ExpectedTrace> const cases{
            {"X trace lies on Y cell plane",
             {{-trace_extent, 0.f, 0.f}},
             {{trace_extent, 0.f, 0.f}},
             1,
             {{-10.f, 0.f, 0.f}}},
            {"Trace ends at origin from positive Y cell",
             {{0.f, trace_extent, 0.f}},
             {{0.f, 0.f, 0.f}},
             1,
             {{0.f, 0.f, 0.f}}},
        };
        check_traces(fixture, cases);
    }

    {
        std::vector<Vector3f> const entity_locations{{{0.f, 0.f, -10.f}}};
        TraceFixture const fixture{entity_locations, aabb_half_extents};
        std::vector<ExpectedTrace> const cases{
            {"X trace lies on Z cell plane",
             {{-trace_extent, 0.f, 0.f}},
             {{trace_extent, 0.f, 0.f}},
             1,
             {{-10.f, 0.f, 0.f}}},
            {"Trace ends at origin from positive Z cell",
             {{0.f, 0.f, trace_extent}},
             {{0.f, 0.f, 0.f}},
             1,
             {{0.f, 0.f, 0.f}}},
        };
        check_traces(fixture, cases);
    }

    {
        std::vector<Vector3f> const entity_locations{{{10.f, -10.f, 0.f}}};
        TraceFixture const fixture{entity_locations, aabb_half_extents};
        std::vector<ExpectedTrace> const cases{
            {"Positive diagonal touches entity at cell edge",
             {{-trace_extent, -trace_extent, 0.f}},
             {{trace_extent, trace_extent, 0.f}},
             1,
             {{0.f, 0.f, 0.f}}},
            {"Negative diagonal touches entity at cell edge",
             {{trace_extent, trace_extent, 0.f}},
             {{-trace_extent, -trace_extent, 0.f}},
             1,
             {{0.f, 0.f, 0.f}}},
        };
        check_traces(fixture, cases);
    }

    {
        std::vector<Vector3f> const entity_locations{{{10.f, -10.f, -10.f}}};
        TraceFixture const fixture{entity_locations, aabb_half_extents};
        std::vector<ExpectedTrace> const cases{
            {"Three-axis diagonal touches entity at cell corner",
             {{-trace_extent, -trace_extent, -trace_extent}},
             {{trace_extent, trace_extent, trace_extent}},
             1,
             {{0.f, 0.f, 0.f}}},
        };
        check_traces(fixture, cases);
    }
}

void CollisionUniformGridTraceRunner::test_short_and_near_parallel_segments() {
    Vector3f const entity_centre{};
    Vector3f const aabb_half_extents{{10.f, 10.f, 10.f}};
    constexpr float face{-10.f};

    std::vector<Vector3f> const entity_locations{entity_centre};
    TraceFixture const fixture{entity_locations, aabb_half_extents};
    std::vector<ExpectedTrace> const cases{
        {"Short segment stops before face", {{-20.f, 0.f, 0.f}}, {{-10.5f, 0.f, 0.f}}, 0},
        {"Short segment stops on face",
         {{-20.f, 0.f, 0.f}},
         {{face, 0.f, 0.f}},
         1,
         {{face, 0.f, 0.f}}},
        {"Short segment crosses face",
         {{-10.25f, 0.f, 0.f}},
         {{-9.75f, 0.f, 0.f}},
         1,
         {{face, 0.f, 0.f}}},
        {"Very short segment remains inside",
         {{0.f, 1.f, 2.f}},
         {{0.001f, 1.f, 2.f}},
         1,
         {{0.f, 1.f, 2.f}}},
        {"Segment remains entirely inside",
         {{-1.f, 2.f, 3.f}},
         {{1.f, 2.f, 3.f}},
         1,
         {{-1.f, 2.f, 3.f}}},
        {"Very short segment remains outside", {{20.f, 0.f, 0.f}}, {{20.001f, 0.f, 0.f}}, 0},
        {"Segment points away from AABB", {{-20.f, 0.f, 0.f}}, {{-21.f, 0.f, 0.f}}, 0},
    };

    check_traces(fixture, cases);
}

void CollisionUniformGridTraceRunner::test_clips_to_grid_bounds() {
    Vector3f const entity_centre{};
    Vector3f const aabb_half_extents{{10.f, 10.f, 10.f}};
    constexpr float grid_extent{400.f};
    constexpr float outside_grid{500.f};
    constexpr float distant_outside_grid{20000.f};

    std::vector<Vector3f> const entity_locations{entity_centre};
    TraceFixture const fixture{entity_locations, aabb_half_extents};
    std::vector<ExpectedTrace> const cases{
        {"X segment crosses grid from outside",
         {{-outside_grid, 0.f, 0.f}},
         {{outside_grid, 0.f, 0.f}},
         1,
         {{-10.f, 0.f, 0.f}}},
        {"Reverse X segment crosses grid from outside",
         {{outside_grid, 0.f, 0.f}},
         {{-outside_grid, 0.f, 0.f}},
         1,
         {{10.f, 0.f, 0.f}}},
        {"Y segment crosses grid from outside",
         {{0.f, -outside_grid, 0.f}},
         {{0.f, outside_grid, 0.f}},
         1,
         {{0.f, -10.f, 0.f}}},
        {"Segment starts on negative grid boundary",
         {{-grid_extent, 0.f, 0.f}},
         {{0.f, 0.f, 0.f}},
         1,
         {{-10.f, 0.f, 0.f}}},
        {"Segment starts on positive grid boundary",
         {{grid_extent, 0.f, 0.f}},
         {{0.f, 0.f, 0.f}},
         1,
         {{10.f, 0.f, 0.f}}},
        {"Outside segment stops at grid before AABB",
         {{-outside_grid, 0.f, 0.f}},
         {{-grid_extent, 0.f, 0.f}},
         0},
        {"Segment remains fully outside grid",
         {{-outside_grid, 0.f, 0.f}},
         {{-outside_grid - 25.f, 0.f, 0.f}},
         0},
        {"Distant X segment crosses grid",
         {{-distant_outside_grid, 0.f, 0.f}},
         {{distant_outside_grid, 0.f, 0.f}},
         1,
         {{-10.f, 0.f, 0.f}}},
        {"Distant segment remains outside negative grid boundary",
         {{-distant_outside_grid, 0.f, 0.f}},
         {{-distant_outside_grid + 1000.f, 0.f, 0.f}},
         0},
        {"Distant segment remains outside positive grid boundary",
         {{distant_outside_grid - 1000.f, 0.f, 0.f}},
         {{distant_outside_grid, 0.f, 0.f}},
         0},
    };

    check_traces(fixture, cases);
}

void CollisionUniformGridTraceRunner::test_degenerate_aabbs() {
    Vector3f const cell_interior{{25.f, 25.f, 25.f}};
    constexpr float trace_offset{25.f};

    {
        Vector3f const point_half_extents{};
        std::vector<Vector3f> const entity_locations{cell_interior};
        TraceFixture const fixture{entity_locations, point_half_extents};
        std::vector<ExpectedTrace> const cases{
            {"Trace crosses point AABB in cell interior",
             {{cell_interior.X - trace_offset, cell_interior.Y, cell_interior.Z}},
             {{cell_interior.X + trace_offset, cell_interior.Y, cell_interior.Z}},
             1,
             cell_interior},
            {"Stationary trace equals point AABB", cell_interior, cell_interior, 1, cell_interior},
            {"Trace misses point AABB",
             {{cell_interior.X - trace_offset, cell_interior.Y + 1.f, cell_interior.Z}},
             {{cell_interior.X + trace_offset, cell_interior.Y + 1.f, cell_interior.Z}},
             0},
        };
        check_traces(fixture, cases);
    }

    {
        Vector3f const plane_half_extents{{0.f, 10.f, 10.f}};
        std::vector<Vector3f> const entity_locations{cell_interior};
        TraceFixture const fixture{entity_locations, plane_half_extents};
        std::vector<ExpectedTrace> const cases{
            {"Trace crosses plane AABB",
             {{cell_interior.X - trace_offset, cell_interior.Y, cell_interior.Z}},
             {{cell_interior.X + trace_offset, cell_interior.Y, cell_interior.Z}},
             1,
             cell_interior},
        };
        check_traces(fixture, cases);
    }

    {
        Vector3f const line_half_extents{{10.f, 0.f, 0.f}};
        std::vector<Vector3f> const entity_locations{cell_interior};
        TraceFixture const fixture{entity_locations, line_half_extents};
        std::vector<ExpectedTrace> const cases{
            {"Trace crosses line AABB",
             {{cell_interior.X, cell_interior.Y - trace_offset, cell_interior.Z}},
             {{cell_interior.X, cell_interior.Y + trace_offset, cell_interior.Z}},
             1,
             cell_interior},
        };
        check_traces(fixture, cases);
    }

    {
        Vector3f const origin{};
        Vector3f const point_half_extents{};
        std::vector<Vector3f> const entity_locations{origin};
        TraceFixture const fixture{entity_locations, point_half_extents};
        std::vector<ExpectedTrace> const cases{
            {"Trace crosses point AABB on grid corner",
             {{-trace_offset, 0.f, 0.f}},
             {{trace_offset, 0.f, 0.f}},
             1,
             origin},
            {"Stationary trace equals point AABB on grid corner", origin, origin, 1, origin},
        };
        check_traces(fixture, cases);
    }
}

void CollisionUniformGridTraceRunner::test_cross_cell_nearest_hit() {
    Vector3f const positive_entity_centre{{150.f, 0.f, 0.f}};
    Vector3f const negative_entity_centre{{-150.f, 0.f, 0.f}};
    Vector3f const aabb_half_extents{{10.f, 10.f, 10.f}};
    constexpr float trace_extent{300.f};

    {
        std::vector<Vector3f> const entity_locations{
            positive_entity_centre,
            negative_entity_centre,
        };
        TraceFixture const fixture{entity_locations, aabb_half_extents};
        std::vector<ExpectedTrace> const cases{
            {"Positive trace returns nearest entity in earlier cell",
             {{-trace_extent, 0.f, 0.f}},
             {{trace_extent, 0.f, 0.f}},
             1,
             {{-160.f, 0.f, 0.f}},
             1},
            {"Negative trace returns nearest entity in earlier cell",
             {{trace_extent, 0.f, 0.f}},
             {{-trace_extent, 0.f, 0.f}},
             1,
             {{160.f, 0.f, 0.f}},
             0},
        };
        check_traces(fixture, cases);
    }

    {
        Vector3f const wide_aabb_half_extents{{160.f, 10.f, 10.f}};
        std::vector<Vector3f> const entity_locations{{{0.f, 0.f, 0.f}}};
        TraceFixture const fixture{entity_locations, wide_aabb_half_extents};
        std::vector<ExpectedTrace> const cases{
            {"AABB repeated across cells returns one stable contact",
             {{-trace_extent, 0.f, 0.f}},
             {{trace_extent, 0.f, 0.f}},
             1,
             {{-160.f, 0.f, 0.f}}},
        };
        check_traces(fixture, cases);
    }
}

void CollisionUniformGridTraceRunner::test_varied_grid_geometry() {
    collision::CellCoord const fixture_grid_dims{5, 7, 3};
    Vector3f const fixture_cell_dims{{80.f, 125.f, 250.f}};
    Vector3f const entity_centre{{25.f, -30.f, 40.f}};
    Vector3f const aabb_half_extents{{15.f, 20.f, 25.f}};

    std::vector<Vector3f> const entity_locations{entity_centre};
    TraceFixture const fixture{
        entity_locations, aabb_half_extents, Vector3f{}, fixture_grid_dims, fixture_cell_dims};
    std::vector<ExpectedTrace> const cases{
        {"Positive X trace in nonuniform grid",
         {{-190.f, -30.f, 40.f}},
         {{190.f, -30.f, 40.f}},
         1,
         {{10.f, -30.f, 40.f}}},
        {"Negative Y trace in nonuniform grid",
         {{25.f, 300.f, 40.f}},
         {{25.f, -300.f, 40.f}},
         1,
         {{25.f, -10.f, 40.f}}},
        {"Positive Z trace in nonuniform grid",
         {{25.f, -30.f, -300.f}},
         {{25.f, -30.f, 300.f}},
         1,
         {{25.f, -30.f, 15.f}}},
        {"Nonuniform-grid trace outside AABB slab",
         {{-190.f, -5.f, 40.f}},
         {{190.f, -5.f, 40.f}},
         0},
    };
    check_traces(fixture, cases);

    Vector3f const boundary_entity_centre{{-55.f, -30.f, 40.f}};
    std::vector<Vector3f> const boundary_entity_locations{boundary_entity_centre};
    TraceFixture const boundary_fixture{boundary_entity_locations,
                                        aabb_half_extents,
                                        Vector3f{},
                                        fixture_grid_dims,
                                        fixture_cell_dims};
    std::vector<ExpectedTrace> const boundary_cases{
        {"Trace follows cell boundary in odd nonuniform grid",
         {{-40.f, -300.f, 40.f}},
         {{-40.f, 300.f, 40.f}},
         1,
         {{-40.f, -50.f, 40.f}}},
    };
    check_traces(boundary_fixture, boundary_cases);

    collision::CellCoord const single_cell_grid_dims{1, 1, 1};
    Vector3f const single_cell_dims{{100.f, 120.f, 140.f}};
    Vector3f const single_cell_half_extents{{10.f, 10.f, 10.f}};
    std::vector<Vector3f> const single_cell_locations{Vector3f{}};
    TraceFixture const single_cell_fixture{single_cell_locations,
                                           single_cell_half_extents,
                                           Vector3f{},
                                           single_cell_grid_dims,
                                           single_cell_dims};
    tests::expect_equal(1, single_cell_fixture.grid.num_cells(), "Single-cell grid has one cell");
    tests::expect_equal(
        1,
        static_cast<std::int32_t>(single_cell_fixture.grid.get_cell_entities({}).size()),
        "Single-cell grid contains its entity");

    std::vector<ExpectedTrace> const single_cell_cases{
        {"Diagonal trace crosses single-cell grid",
         {{-100.f, -100.f, -100.f}},
         {{100.f, 100.f, 100.f}},
         1,
         {{-10.f, -10.f, -10.f}}},
        {"Stationary trace hits inside single-cell grid", Vector3f{}, Vector3f{}, 1, Vector3f{}},
        {"Trace enters single-cell grid from positive boundary",
         {{50.f, 0.f, 0.f}},
         Vector3f{},
         1,
         {{10.f, 0.f, 0.f}}},
        {"Trace along excluded single-cell positive boundary",
         {{50.f, -20.f, 0.f}},
         {{50.f, 20.f, 0.f}},
         0},
    };
    check_traces(single_cell_fixture, single_cell_cases);
}

void CollisionUniformGridTraceRunner::test_boundary_precision() {
    Vector3f const entity_centre{};
    Vector3f const aabb_half_extents{{10.f, 10.f, 10.f}};
    constexpr float face{10.f};
    constexpr float negative_face{-face};
    constexpr float trace_extent{20.f};
    auto const inside_face{std::nextafter(face, 0.f)};
    auto const outside_face{std::nextafter(face, std::numeric_limits<float>::infinity())};
    auto const before_negative_face{
        std::nextafter(negative_face, -std::numeric_limits<float>::infinity())};
    auto const after_negative_face{std::nextafter(negative_face, 0.f)};

    std::vector<Vector3f> const entity_locations{entity_centre};
    TraceFixture const fixture{entity_locations, aabb_half_extents};
    std::vector<ExpectedTrace> const cases{
        {"Parallel trace exactly on face",
         {{-trace_extent, face, 0.f}},
         {{trace_extent, face, 0.f}},
         1,
         {{negative_face, face, 0.f}}},
        {"Parallel trace one float inside face",
         {{-trace_extent, inside_face, 0.f}},
         {{trace_extent, inside_face, 0.f}},
         1,
         {{negative_face, inside_face, 0.f}}},
        {"Parallel trace one float outside face",
         {{-trace_extent, outside_face, 0.f}},
         {{trace_extent, outside_face, 0.f}},
         0},
        {"Endpoint one float before face",
         {{-trace_extent, 0.f, 0.f}},
         {{before_negative_face, 0.f, 0.f}},
         0},
        {"Endpoint exactly on face",
         {{-trace_extent, 0.f, 0.f}},
         {{negative_face, 0.f, 0.f}},
         1,
         {{negative_face, 0.f, 0.f}}},
        {"Endpoint one float beyond face",
         {{-trace_extent, 0.f, 0.f}},
         {{after_negative_face, 0.f, 0.f}},
         1,
         {{negative_face, 0.f, 0.f}}},
    };
    check_traces(fixture, cases);
}

void CollisionUniformGridTraceRunner::test_rebuild_lifecycle() {
    Vector3f const initial_location{{-150.f, 0.f, 0.f}};
    Vector3f const moved_location{{150.f, 0.f, 0.f}};
    Vector3f const aabb_half_extents{{10.f, 10.f, 10.f}};
    constexpr float trace_offset{25.f};

    std::vector<Vector3f> const initial_locations{initial_location};
    TraceFixture fixture{initial_locations, aabb_half_extents};
    for (std::int32_t rebuild{}; rebuild < 4; ++rebuild) {
        fixture.grid.rebuild_grid(fixture.aabbs);
    }

    std::vector<ExpectedTrace> const initial_cases{
        {"Repeated rebuild retains entity",
         {{initial_location.X - trace_offset, 0.f, 0.f}},
         {{initial_location.X + trace_offset, 0.f, 0.f}},
         1,
         {{initial_location.X - aabb_half_extents.X, 0.f, 0.f}}},
    };
    check_traces(fixture, initial_cases);

    std::vector<Vector3f> const moved_locations{moved_location};
    std::vector<std::uint8_t> const alive{std::uint8_t{1}};
    fixture.update_entities(moved_locations, alive);
    std::vector<ExpectedTrace> const moved_cases{
        {"Moved entity is removed from old cells",
         {{initial_location.X - trace_offset, 0.f, 0.f}},
         {{initial_location.X + trace_offset, 0.f, 0.f}},
         0},
        {"Moved entity is added to new cells",
         {{moved_location.X - trace_offset, 0.f, 0.f}},
         {{moved_location.X + trace_offset, 0.f, 0.f}},
         1,
         {{moved_location.X - aabb_half_extents.X, 0.f, 0.f}}},
    };
    check_traces(fixture, moved_cases);

    std::vector<std::uint8_t> const dead{std::uint8_t{0}};
    fixture.update_entities(moved_locations, dead);
    std::vector<ExpectedTrace> const dead_cases{
        {"Dead entity is removed on rebuild",
         {{moved_location.X - trace_offset, 0.f, 0.f}},
         {{moved_location.X + trace_offset, 0.f, 0.f}},
         0},
    };
    check_traces(fixture, dead_cases);

    Vector3f const replacement_location{{250.f, 0.f, 0.f}};
    auto const old_handle{fixture.handles[0]};
    auto const replacement_handle{fixture.add_entity(replacement_location)};
    tests::expect_equal(
        old_handle.index, replacement_handle.index, "Replacement entity reuses dead registry slot");
    tests::expect_true(old_handle.generation != replacement_handle.generation,
                       "Replacement entity advances registry generation");
    tests::expect_true(fixture.registry.is_stale(old_handle),
                       "Reused collision handle becomes stale");

    std::vector<Vector3f> const replacement_starts{
        {{replacement_location.X - trace_offset, 0.f, 0.f}},
    };
    std::vector<Vector3f> const replacement_ends{
        {{replacement_location.X + trace_offset, 0.f, 0.f}},
    };
    auto const replacement_hits{run_traces(fixture, replacement_starts, replacement_ends)};
    tests::expect_equal(
        std::uint8_t{1}, replacement_hits.hits[0], "Replacement entity is added on rebuild");
    if (replacement_hits.hits[0] != 0) {
        tests::expect_equal(replacement_handle,
                            replacement_hits.entities[0],
                            "Trace resolves replacement generation");
    }

    std::vector<Vector3f> const sparse_locations{
        {{-250.f, 0.f, 0.f}},
        {{0.f, 0.f, 0.f}},
        {{250.f, 0.f, 0.f}},
    };
    TraceFixture sparse_fixture{sparse_locations, aabb_half_extents};
    std::vector<std::uint8_t> const sparse_alive{std::uint8_t{1}, std::uint8_t{0}, std::uint8_t{1}};
    sparse_fixture.update_entities(sparse_locations, sparse_alive);
    std::vector<ExpectedTrace> const sparse_cases{
        {"Live entity before dead slot remains traceable",
         {{-250.f - trace_offset, 0.f, 0.f}},
         {{-250.f + trace_offset, 0.f, 0.f}},
         1,
         {{-250.f - aabb_half_extents.X, 0.f, 0.f}},
         0},
        {"Dead middle registry slot is omitted",
         {{-trace_offset, 0.f, 0.f}},
         {{trace_offset, 0.f, 0.f}},
         0},
        {"Live entity after dead slot remains traceable",
         {{250.f - trace_offset, 0.f, 0.f}},
         {{250.f + trace_offset, 0.f, 0.f}},
         1,
         {{250.f - aabb_half_extents.X, 0.f, 0.f}},
         2},
    };
    check_traces(sparse_fixture, sparse_cases);

    Vector3f const sparse_replacement_location{{0.f, 200.f, 0.f}};
    auto const sparse_old_handle{sparse_fixture.handles[1]};
    auto const sparse_replacement_handle{sparse_fixture.add_entity(sparse_replacement_location)};
    tests::expect_equal(sparse_old_handle.index,
                        sparse_replacement_handle.index,
                        "Sparse replacement reuses middle registry slot");
    tests::expect_true(sparse_old_handle.generation != sparse_replacement_handle.generation,
                       "Sparse replacement advances middle registry generation");

    std::vector<Vector3f> const sparse_replacement_starts{
        sparse_replacement_location - Vector3f{{0.f, trace_offset, 0.f}},
    };
    std::vector<Vector3f> const sparse_replacement_ends{
        sparse_replacement_location + Vector3f{{0.f, trace_offset, 0.f}},
    };
    auto const sparse_replacement_hits{
        run_traces(sparse_fixture, sparse_replacement_starts, sparse_replacement_ends)};
    tests::expect_equal(std::uint8_t{1},
                        sparse_replacement_hits.hits[0],
                        "Sparse replacement remains traceable beside surviving entities");
    if (sparse_replacement_hits.hits[0] != 0) {
        tests::expect_equal(sparse_replacement_handle,
                            sparse_replacement_hits.entities[0],
                            "Sparse replacement trace resolves new generation");
    }
}

void CollisionUniformGridTraceRunner::test_deterministic_reference_sweep() {
    std::array<std::int32_t, 3> const seeds{0x51A8B3, 0x19C0DE, 0x7A11CE};
    constexpr std::int32_t entity_count{32};
    constexpr std::int32_t trace_count{512};
    constexpr float entity_extent{320.f};
    constexpr float trace_extent{800.f};
    Vector3f const targeted_trace_extent{{1000.f, 1000.f, 1000.f}};
    Vector3f const local_aabb_centre{{3.f, -5.f, 7.f}};
    Vector3f const aabb_half_extents{{7.f, 11.f, 13.f}};

    auto const run_sweep{[local_aabb_centre, aabb_half_extents, targeted_trace_extent](
                             std::int32_t const seed, std::int32_t const case_offset) {
        std::uint32_t random_state{static_cast<std::uint32_t>(seed)};
        auto const random_range{[&random_state](float const extent) {
            // Preserve the original Unreal test stream and its double-precision range scaling.
            random_state = random_state * 196314165u + 907633515u;
            auto const fraction{std::bit_cast<float>(0x3f800000u | (random_state >> 9)) - 1.f};
            return static_cast<float>(-static_cast<double>(extent) + 2.0 * extent * fraction);
        }};
        std::vector<Vector3f> entity_locations{};
        entity_locations.reserve(entity_count);
        for (std::int32_t i{}; i < entity_count; ++i) {
            entity_locations.push_back({{random_range(entity_extent),
                                         random_range(entity_extent),
                                         random_range(entity_extent)}});
        }

        TraceFixture const fixture{entity_locations, aabb_half_extents, local_aabb_centre};
        std::vector<Vector3f> starts{};
        std::vector<Vector3f> ends{};
        starts.reserve(trace_count);
        ends.reserve(trace_count);
        for (std::int32_t i{}; i < entity_count; ++i) {
            auto const entity_location{entity_locations[i]};
            auto const world_centre{entity_location + local_aabb_centre};
            auto const start_offset{i % 2 == 0 ? -targeted_trace_extent : targeted_trace_extent};
            starts.push_back(world_centre + start_offset);
            ends.push_back(world_centre - start_offset);
        }
        for (std::int32_t i{entity_count}; i < trace_count; ++i) {
            starts.push_back({{random_range(trace_extent),
                               random_range(trace_extent),
                               random_range(trace_extent)}});
            ends.push_back({{random_range(trace_extent),
                             random_range(trace_extent),
                             random_range(trace_extent)}});
        }

        auto const hits{run_traces(fixture, starts, ends)};
        std::int32_t expected_hit_count{};
        for (std::int32_t i_trace{}; i_trace < trace_count; ++i_trace) {
            auto nearest_t{std::numeric_limits<float>::infinity()};
            std::int32_t nearest_entity{-1};
            for (std::int32_t i_entity{}; i_entity < entity_count; ++i_entity) {
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

            auto const expected_hit{std::uint8_t{std::isfinite(nearest_t)}};
            expected_hit_count += expected_hit;
            auto const case_index{case_offset + i_trace};
            tests::expect_equal(expected_hit,
                                hits.hits[i_trace],
                                "Reference sweep trace has expected hit flag",
                                case_index);
            if (expected_hit == 0 || hits.hits[i_trace] == 0) {
                continue;
            }

            tests::expect_equal(fixture.handles[nearest_entity],
                                hits.entities[i_trace],
                                "Reference sweep trace resolves nearest entity",
                                case_index);
            auto const expected_location{
                (starts[i_trace] + (ends[i_trace] - starts[i_trace]) * nearest_t)};
            tests::expect_distance_near(expected_location,
                                        hits.locations[i_trace],
                                        hit_location_tolerance,
                                        "Reference sweep trace resolves nearest location",
                                        case_index);
        }

        tests::expect_true(expected_hit_count >= entity_count,
                           "Reference sweep contains targeted hits");
        tests::expect_true(expected_hit_count < trace_count, "Reference sweep contains misses");
    }};

    auto const seed_count{static_cast<std::int32_t>(seeds.size())};
    for (std::int32_t i{}; i < seed_count; ++i) {
        run_sweep(seeds[i], i * trace_count);
    }
}

void CollisionUniformGridTraceRunner::test_invariance_properties() {
    Vector3f const aabb_half_extents{{18.f, 22.f, 15.f}};
    Vector3f const local_aabb_centre{{4.f, -3.f, 5.f}};
    std::vector<Vector3f> const entity_locations{
        {{-220.f, -40.f, 10.f}},
        {{35.f, 70.f, -20.f}},
        {{210.f, -120.f, 80.f}},
    };
    std::vector<Vector3f> const starts{
        {{-280.f, -43.f, 15.f}},
        {{-21.f, 67.f, -15.f}},
        {{154.f, -123.f, 85.f}},
        {{-280.f, 150.f, 15.f}},
        {{35.f, -200.f, -15.f}},
        {{-300.f, -300.f, -300.f}},
    };
    std::vector<Vector3f> const ends{
        {{-160.f, -43.f, 15.f}},
        {{99.f, 67.f, -15.f}},
        {{274.f, -123.f, 85.f}},
        {{-160.f, 150.f, 15.f}},
        {{35.f, -100.f, -15.f}},
        {{-250.f, -250.f, -250.f}},
    };

    TraceFixture const baseline_fixture{entity_locations, aabb_half_extents, local_aabb_centre};
    auto const baseline_hits{run_traces(baseline_fixture, starts, ends)};
    auto const compare_same_entity_order{[&baseline_hits](TraceHits const& candidate_hits,
                                                          char const* const description) {
        auto const count{baseline_hits.num()};
        for (std::int32_t i{}; i < count; ++i) {
            tests::expect_equal(baseline_hits.hits[i], candidate_hits.hits[i], description, i);
            if (baseline_hits.hits[i] == 0 || candidate_hits.hits[i] == 0) {
                continue;
            }
            tests::expect_equal(
                baseline_hits.entities[i].index, candidate_hits.entities[i].index, description, i);
            tests::expect_distance_near(baseline_hits.locations[i],
                                        candidate_hits.locations[i],
                                        hit_location_tolerance,
                                        description,
                                        i);
        }
    }};

    TraceFixture const coarse_fixture{
        entity_locations, aabb_half_extents, local_aabb_centre, {4, 4, 4}, {{200.f, 200.f, 200.f}}};
    auto const coarse_hits{run_traces(coarse_fixture, starts, ends)};
    compare_same_entity_order(coarse_hits, "Coarse grid preserves trace results");

    TraceFixture const fine_fixture{
        entity_locations, aabb_half_extents, local_aabb_centre, {16, 16, 16}, {{50.f, 50.f, 50.f}}};
    auto const fine_hits{run_traces(fine_fixture, starts, ends)};
    compare_same_entity_order(fine_hits, "Fine grid preserves trace results");

    std::array<std::int32_t, 6> const permutation{4, 1, 5, 0, 3, 2};
    std::vector<Vector3f> permuted_starts{};
    std::vector<Vector3f> permuted_ends{};
    for (auto const source_index : permutation) {
        permuted_starts.push_back(starts[source_index]);
        permuted_ends.push_back(ends[source_index]);
    }
    auto const permuted_hits{run_traces(baseline_fixture, permuted_starts, permuted_ends)};
    auto const trace_count{static_cast<std::int32_t>(permutation.size())};
    for (std::int32_t i{}; i < trace_count; ++i) {
        auto const source_index{permutation[i]};
        tests::expect_equal(baseline_hits.hits[source_index],
                            permuted_hits.hits[i],
                            "Trace permutation preserves hit flag",
                            i);
        if (baseline_hits.hits[source_index] == 0 || permuted_hits.hits[i] == 0) {
            continue;
        }
        tests::expect_equal(baseline_hits.entities[source_index],
                            permuted_hits.entities[i],
                            "Trace permutation preserves entity",
                            i);
        tests::expect_distance_near(baseline_hits.locations[source_index],
                                    permuted_hits.locations[i],
                                    hit_location_tolerance,
                                    "Trace permutation preserves location",
                                    i);
    }

    std::vector<Vector3f> const reversed_entity_locations{
        entity_locations[2],
        entity_locations[1],
        entity_locations[0],
    };
    TraceFixture const reversed_fixture{
        reversed_entity_locations, aabb_half_extents, local_aabb_centre};
    auto const reversed_hits{run_traces(reversed_fixture, starts, ends)};
    for (std::int32_t i{}; i < trace_count; ++i) {
        tests::expect_equal(baseline_hits.hits[i],
                            reversed_hits.hits[i],
                            "Entity insertion order preserves hit flag",
                            i);
        if (baseline_hits.hits[i] != 0 && reversed_hits.hits[i] != 0) {
            tests::expect_distance_near(baseline_hits.locations[i],
                                        reversed_hits.locations[i],
                                        hit_location_tolerance,
                                        "Entity insertion order preserves nearest location",
                                        i);
        }
    }

    Vector3f const translation{{100.f, 0.f, 0.f}};
    std::vector<Vector3f> translated_entity_locations{};
    std::vector<Vector3f> translated_starts{};
    std::vector<Vector3f> translated_ends{};
    for (auto const location : entity_locations) {
        translated_entity_locations.push_back(location + translation);
    }
    for (std::int32_t i{}; i < trace_count; ++i) {
        translated_starts.push_back(starts[i] + translation);
        translated_ends.push_back(ends[i] + translation);
    }
    TraceFixture const translated_fixture{
        translated_entity_locations, aabb_half_extents, local_aabb_centre};
    auto const translated_hits{run_traces(translated_fixture, translated_starts, translated_ends)};
    for (std::int32_t i{}; i < trace_count; ++i) {
        tests::expect_equal(baseline_hits.hits[i],
                            translated_hits.hits[i],
                            "Whole-cell translation preserves hit flag",
                            i);
        if (baseline_hits.hits[i] != 0 && translated_hits.hits[i] != 0) {
            tests::expect_equal(baseline_hits.entities[i].index,
                                translated_hits.entities[i].index,
                                "Whole-cell translation preserves entity",
                                i);
            tests::expect_distance_near(baseline_hits.locations[i] + translation,
                                        translated_hits.locations[i],
                                        hit_location_tolerance,
                                        "Whole-cell translation preserves location",
                                        i);
        }
    }
}

void CollisionUniformGridTraceRunner::test_empty_batches_and_output_reuse() {
    Vector3f const aabb_half_extents{{10.f, 10.f, 10.f}};
    std::vector<Vector3f> const no_entities;
    TraceFixture const empty_fixture{no_entities, aabb_half_extents};
    std::vector<Vector3f> const no_traces;
    auto const no_results{run_traces(empty_fixture, no_traces, no_traces)};
    tests::expect_equal(0, no_results.num(), "Empty grid accepts empty trace batch");

    std::vector<Vector3f> const miss_starts{{{-20.f, 0.f, 0.f}}, {{0.f, -20.f, 0.f}}};
    std::vector<Vector3f> const miss_ends{{{20.f, 0.f, 0.f}}, {{0.f, 20.f, 0.f}}};
    auto const empty_grid_hits{run_traces(empty_fixture, miss_starts, miss_ends)};
    tests::expect_equal(std::uint8_t{0}, empty_grid_hits.hits[0], "Empty grid misses first trace");
    tests::expect_equal(std::uint8_t{0}, empty_grid_hits.hits[1], "Empty grid misses second trace");

    std::vector<Vector3f> const entity_locations{Vector3f{}};
    TraceFixture const populated_fixture{entity_locations, aabb_half_extents};
    auto const empty_batch_hits{run_traces(populated_fixture, no_traces, no_traces)};
    tests::expect_equal(0, empty_batch_hits.num(), "Populated grid accepts empty trace batch");

    TraceHits reused_hits;
    reused_hits.add_defaulted(1);
    LineTraces hit_trace;
    hit_trace.starts.add({{-20.f, 0.f, 0.f}});
    hit_trace.ends.add({{20.f, 0.f, 0.f}});
    populated_fixture.grid.trace_aabbs(hit_trace.get_const_view(), reused_hits.get_view());
    tests::expect_equal(
        std::uint8_t{1}, reused_hits.hits[0], "Reused output initially records hit");

    LineTraces miss_trace;
    miss_trace.starts.add({{-20.f, 20.f, 0.f}});
    miss_trace.ends.add({{20.f, 20.f, 0.f}});
    populated_fixture.grid.trace_aabbs(miss_trace.get_const_view(), reused_hits.get_view());
    tests::expect_equal(
        std::uint8_t{0}, reused_hits.hits[0], "Reused output clears stale hit flag");

    populated_fixture.grid.trace_aabbs(hit_trace.get_const_view(), reused_hits.get_view());
    tests::expect_equal(std::uint8_t{1}, reused_hits.hits[0], "Reused output records later hit");
    tests::expect_equal(populated_fixture.handles[0],
                        reused_hits.entities[0],
                        "Reused output records later entity");
}

void CollisionUniformGridTraceRunner::test_dense_and_wide_aabbs() {
    constexpr std::int32_t dense_entity_count{2048};
    Vector3f const dense_location{{25.f, 25.f, 25.f}};
    Vector3f const dense_half_extents{{1.f, 1.f, 1.f}};
    std::vector<Vector3f> dense_locations{};
    dense_locations.assign(dense_entity_count, dense_location);
    TraceFixture const dense_fixture{dense_locations, dense_half_extents};
    auto const dense_cell{dense_fixture.grid.to_cell_coord(dense_location)};
    tests::expect_equal(
        dense_entity_count,
        static_cast<std::int32_t>(dense_fixture.grid.get_cell_entities(dense_cell).size()),
        "Dense cell retains every entity");

    std::vector<ExpectedTrace> const dense_cases{
        {"Trace resolves dense cell",
         {{20.f, 25.f, 25.f}},
         {{30.f, 25.f, 25.f}},
         1,
         {{24.f, 25.f, 25.f}}},
    };
    check_traces(dense_fixture, dense_cases);

    Vector3f const wide_half_extents{{160.f, 160.f, 160.f}};
    std::vector<Vector3f> const wide_locations{Vector3f{}};
    TraceFixture const wide_fixture{wide_locations, wide_half_extents};
    auto const [min_coord, max_coord]{
        wide_fixture.grid.to_cell_coord_bounds(-wide_half_extents, wide_half_extents)};
    std::int32_t membership_count{};
    for (std::int32_t x{min_coord.x}; x <= max_coord.x; ++x) {
        for (std::int32_t y{min_coord.y}; y <= max_coord.y; ++y) {
            for (std::int32_t z{min_coord.z}; z <= max_coord.z; ++z) {
                membership_count += count_handle(wide_fixture.grid.get_cell_entities({x, y, z}),
                                                 wide_fixture.handles[0]);
            }
        }
    }
    tests::expect_equal(64, membership_count, "Three-axis AABB occupies every covered cell");

    std::vector<ExpectedTrace> const wide_cases{
        {"Diagonal trace resolves three-axis multi-cell AABB",
         {{-300.f, -300.f, -300.f}},
         {{300.f, 300.f, 300.f}},
         1,
         {{-160.f, -160.f, -160.f}}},
    };
    check_traces(wide_fixture, wide_cases);
}

void CollisionUniformGridTraceRunner::test_production_scale() {
    constexpr float grid_boundary{1000000.f};
    auto const inside_positive_grid_boundary{std::nextafter(grid_boundary, 0.f)};
    Vector3f const aabb_half_extents{{1000.f, 1000.f, 1000.f}};
    Vector3f const negative_entity{{-999000.f, 0.f, 0.f}};
    Vector3f const positive_entity{{999000.f, 0.f, 0.f}};
    std::vector<Vector3f> const entity_locations{negative_entity, positive_entity};
    TraceFixture const fixture{
        entity_locations, aabb_half_extents, Vector3f{}, grid_dims, cell_dims};
    std::vector<ExpectedTrace> const cases{
        {"Trace starts on negative production grid boundary",
         {{-grid_boundary, 0.f, 0.f}},
         {{-990000.f, 0.f, 0.f}},
         1,
         {{-grid_boundary, 0.f, 0.f}},
         0},
        {"Trace ends on positive production grid boundary",
         {{990000.f, 0.f, 0.f}},
         {{grid_boundary, 0.f, 0.f}},
         1,
         {{998000.f, 0.f, 0.f}},
         1},
        {"Trace follows negative production grid boundary",
         {{-grid_boundary, -5000.f, 0.f}},
         {{-grid_boundary, 5000.f, 0.f}},
         1,
         {{-grid_boundary, -1000.f, 0.f}},
         0},
        {"Trace follows positive production grid boundary",
         {{grid_boundary, -5000.f, 0.f}},
         {{grid_boundary, 5000.f, 0.f}},
         0},
        {"Trace follows one float inside positive production grid boundary",
         {{inside_positive_grid_boundary, -5000.f, 0.f}},
         {{inside_positive_grid_boundary, 5000.f, 0.f}},
         1,
         {{inside_positive_grid_boundary, -1000.f, 0.f}},
         1},
        {"Outside trace only touches positive production grid boundary",
         {{grid_boundary + 10000.f, 0.f, 0.f}},
         {{grid_boundary, 0.f, 0.f}},
         0},
    };
    check_traces(fixture, cases);
}

void CollisionUniformGridTraceRunner::test_static_geometry() {
    Vector3f const dynamic_location{{100.f, 0.f, 0.f}};
    Vector3f const half_extents{{10.f, 10.f, 10.f}};
    std::vector<Vector3f> const dynamic_locations{dynamic_location};
    TraceFixture fixture{dynamic_locations, half_extents};

    auto set_static_aabb{[&fixture](Vector3f const min_point, Vector3f const max_point) {
        collision::WorldAABBs static_aabbs;
        collision::add(static_aabbs, min_point, max_point);
        fixture.grid.set_static_aabbs(std::move(static_aabbs));
    }};
    std::vector<Vector3f> const starts{{{-200.f, 0.f, 0.f}}};
    std::vector<Vector3f> const ends{{{200.f, 0.f, 0.f}}};

    set_static_aabb({{-60.f, -10.f, -10.f}}, {{-40.f, 10.f, 10.f}});
    auto static_hits{run_traces(fixture, starts, ends)};
    tests::expect_equal(std::uint8_t{1}, static_hits.hits[0], "Static AABB is traceable");
    tests::expect_true(!static_hits.entities[0].is_valid(), "Static hit has no dynamic entity");
    tests::expect_equal(0,
                        static_hits.static_geometry_indices[0],
                        "Static hit identifies canonical static geometry");
    tests::expect_distance_near(Vector3f{{-60.f, 0.f, 0.f}},
                                static_hits.locations[0],
                                hit_location_tolerance,
                                "Static hit reports nearest entry point");

    set_static_aabb({{-60.f, 100.f, -10.f}}, {{-40.f, 120.f, 10.f}});
    std::vector<Vector3f> const offset_starts{{{-200.f, 80.f, 0.f}}};
    std::vector<Vector3f> const offset_ends{{{200.f, 80.f, 0.f}}};
    auto const offset_hits{run_traces(fixture, offset_starts, offset_ends)};
    tests::expect_equal(std::uint8_t{0}, offset_hits.hits[0], "Offset line misses static geometry");
    auto const sweep_hits{
        run_sweeps(fixture, offset_starts, offset_ends, Vector3f{{20.f, 20.f, 20.f}})};
    tests::expect_equal(std::uint8_t{1}, sweep_hits.hits[0], "AABB sweep detects static geometry");
    tests::expect_distance_near(Vector3f{{-80.f, 80.f, 0.f}},
                                sweep_hits.locations[0],
                                hit_location_tolerance,
                                "AABB sweep reports expanded entry point");

    std::vector<Vector3f> const fighter_locations{{{-100.f, 0.f, 0.f}}};
    std::vector<EntityType> const fighter_types{EntityType::Fighter};
    TraceFixture fighter_fixture{fighter_locations,
                                 half_extents,
                                 Vector3f{},
                                 trace_grid_dims,
                                 trace_cell_dims,
                                 fighter_types};
    fighter_fixture.set_entity_aabb(EntityType::Fighter, Vector3f{}, half_extents);
    collision::WorldAABBs blocked_static_aabbs;
    collision::add(blocked_static_aabbs, {{50.f, -10.f, -10.f}}, {{70.f, 10.f, 10.f}});
    fighter_fixture.grid.set_static_aabbs(std::move(blocked_static_aabbs));

    auto const fighter_masked_hits{
        run_sweeps(fighter_fixture, starts, ends, Vector3f{{20.f, 20.f, 20.f}})};
    tests::expect_equal(fighter_fixture.handles[0],
                        fighter_masked_hits.entities[0],
                        "Fighter is the closest sweep hit before static geometry");
    auto const static_geometry_hits{run_sweeps(fighter_fixture,
                                               starts,
                                               ends,
                                               Vector3f{{20.f, 20.f, 20.f}},
                                               {},
                                               collision::TraceEntityFilter::ExcludeFighters)};
    tests::expect_equal(std::uint8_t{1},
                        static_geometry_hits.hits[0],
                        "Static geometry remains after fighter exclusion");
    tests::expect_true(!static_geometry_hits.entities[0].is_valid(),
                       "Fighter exclusion returns the static obstacle");
    tests::expect_equal(0,
                        static_geometry_hits.static_geometry_indices[0],
                        "Fighter exclusion keeps the static obstacle identity");

    set_static_aabb({{-60.f, -10.f, -10.f}}, {{-40.f, 10.f, 10.f}});
    fixture.grid.rebuild_grid(fixture.aabbs);
    auto const rebuilt_static_hits{run_traces(fixture, starts, ends)};
    tests::expect_equal(0,
                        rebuilt_static_hits.static_geometry_indices[0],
                        "Static geometry survives dynamic rebuild");

    set_static_aabb({{140.f, -10.f, -10.f}}, {{160.f, 10.f, 10.f}});
    auto const dynamic_hits{run_traces(fixture, starts, ends)};
    tests::expect_equal(fixture.handles[0],
                        dynamic_hits.entities[0],
                        "Closer dynamic geometry wins over static geometry");
    tests::expect_equal(std::int32_t{-1},
                        dynamic_hits.static_geometry_indices[0],
                        "Dynamic hit clears static identity");

    std::vector<RegistryEntityHandle> const ignored_entities{fixture.handles[0]};
    auto const ignored_dynamic_hits{run_traces(fixture, starts, ends, ignored_entities)};
    tests::expect_true(!ignored_dynamic_hits.entities[0].is_valid(),
                       "Ignored dynamic entity is not returned");
    tests::expect_equal(0,
                        ignored_dynamic_hits.static_geometry_indices[0],
                        "Static geometry behind ignored entity remains traceable");

    set_static_aabb({{90.f, -10.f, -10.f}}, {{110.f, 10.f, 10.f}});
    auto const tied_hits{run_traces(fixture, starts, ends)};
    tests::expect_equal(fixture.handles[0],
                        tied_hits.entities[0],
                        "Dynamic geometry wins exact-distance static tie");

    set_static_aabb({{140.f, -10.f, -10.f}}, {{160.f, 10.f, 10.f}});
    auto const runtime_static_index{
        fixture.grid.add_static_aabb({{-60.f, -10.f, -10.f}}, {{-40.f, 10.f, 10.f}})};
    tests::expect_equal(1, runtime_static_index, "Runtime static AABB receives stable identity");
    auto const runtime_static_hits{run_traces(fixture, starts, ends)};
    tests::expect_equal(runtime_static_index,
                        runtime_static_hits.static_geometry_indices[0],
                        "Runtime static AABB is immediately traceable");

    fixture.grid.rebuild_grid(fixture.aabbs);
    auto const rebuilt_runtime_static_hits{run_traces(fixture, starts, ends)};
    tests::expect_equal(runtime_static_index,
                        rebuilt_runtime_static_hits.static_geometry_indices[0],
                        "Runtime static AABB survives dynamic rebuild");
}

void CollisionUniformGridTraceRunner::run() {
    switch (scenario_) {
        case CollisionUniformGridTraceScenario::HitsAndMisses:
            test_hits_and_misses();
            break;
        case CollisionUniformGridTraceScenario::StopsAtEndpoint:
            test_stops_at_endpoint();
            break;
        case CollisionUniformGridTraceScenario::ReturnsNearestHit:
            test_returns_nearest_hit();
            break;
        case CollisionUniformGridTraceScenario::HandlesZeroLengthTraces:
            test_handles_zero_length_traces();
            break;
        case CollisionUniformGridTraceScenario::IncludesNegativeEndpointBoundary:
            test_includes_negative_endpoint_boundary();
            break;
        case CollisionUniformGridTraceScenario::AppliesAABBCentre:
            test_applies_aabb_centre();
            break;
        case CollisionUniformGridTraceScenario::AxisParallelAndOrigin:
            test_axis_parallel_and_origin();
            break;
        case CollisionUniformGridTraceScenario::SurfaceContacts:
            test_surface_contacts();
            break;
        case CollisionUniformGridTraceScenario::GridBoundaryTraversal:
            test_grid_boundary_traversal();
            break;
        case CollisionUniformGridTraceScenario::ShortAndNearParallelSegments:
            test_short_and_near_parallel_segments();
            break;
        case CollisionUniformGridTraceScenario::ClipsToGridBounds:
            test_clips_to_grid_bounds();
            break;
        case CollisionUniformGridTraceScenario::DegenerateAABBs:
            test_degenerate_aabbs();
            break;
        case CollisionUniformGridTraceScenario::CrossCellNearestHit:
            test_cross_cell_nearest_hit();
            break;
        case CollisionUniformGridTraceScenario::VariedGridGeometry:
            test_varied_grid_geometry();
            break;
        case CollisionUniformGridTraceScenario::BoundaryPrecision:
            test_boundary_precision();
            break;
        case CollisionUniformGridTraceScenario::RebuildLifecycle:
            test_rebuild_lifecycle();
            break;
        case CollisionUniformGridTraceScenario::DeterministicReferenceSweep:
            test_deterministic_reference_sweep();
            break;
        case CollisionUniformGridTraceScenario::InvarianceProperties:
            test_invariance_properties();
            break;
        case CollisionUniformGridTraceScenario::EmptyBatchesAndOutputReuse:
            test_empty_batches_and_output_reuse();
            break;
        case CollisionUniformGridTraceScenario::DenseAndWideAABBs:
            test_dense_and_wide_aabbs();
            break;
        case CollisionUniformGridTraceScenario::ProductionScale:
            test_production_scale();
            break;
        case CollisionUniformGridTraceScenario::StaticGeometry:
            test_static_geometry();
            break;
    }
}

void run_collision_uniform_grid_trace(tests::SimulationFixture const&,
                                      CollisionUniformGridTraceScenario const scenario) {
    CollisionUniformGridTraceRunner runner{scenario};
    runner.run();
}
}
