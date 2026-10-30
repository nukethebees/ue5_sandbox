#include "support/collision_agent_storage.h"
#include "support/simulation_checks.h"
#include "support/simulation_test_support.h"
#include <ioj/column_math.h>
#include <ioj/frame_memory_resource.h>
#include <ioj/sim/collision/spatial_query_manager.h>
#include <ioj/sim/memory/game_memory.h>
#include <ioj/sim/testing/entity_observations.h>
#include <ioj/sim/testing/spatial_query_manager_test_access.h>

#include <array>
#include <cstddef>
#include <initializer_list>
#include <optional>

namespace ioj::sim::tests {

namespace {
constexpr std::size_t overlap_frame_memory_capacity{4 * 1024 * 1024};

struct EntityMove {
    EntityId id;
    Vector3f location;
    Rotator3f rotation{};
    bool alive{true};
};

struct OverlapFixture {
    explicit OverlapFixture(Vector3f const capital_half_extents = {{10.f, 10.f, 10.f}},
                            Vector3f const capital_centre = Vector3f{},
                            Vector3f const turret_half_extents = {{10.f, 10.f, 10.f}})
        : query_manager{owners.entity_tables} {
        for (auto const type : EnumTraits<EntityType>::values) {
            set_bounds(type, Vector3f{}, Vector3f{{10.f, 10.f, 10.f}});
        }
        set_bounds(EntityType::CapitalShip, capital_centre, capital_half_extents);
        set_bounds(EntityType::Turret, Vector3f{}, turret_half_extents);

        query_manager.initialise({{40, 40, 40}, {{50.f, 50.f, 50.f}}}, entity_bounds);
    }

    void refresh_and_detect_overlaps(std::span<EntityId const> const overlap_candidates) {
        owners.refresh(query_manager);
        {
            FrameScratchScope scratch_scope{frame_memory};
            try {
                query_manager.detect_overlaps(overlap_candidates, &frame_memory);
            } catch (std::bad_alloc const&) {
                auto const stats{frame_memory.get_stats()};
                ADD_FAILURE() << "Frame overflow count: " << stats.overflow_count
                              << ", requested: " << stats.last_failure.requested_bytes
                              << ", claimed: " << stats.last_failure.claimed_bytes;
                throw;
            }
        }

        auto const stats{frame_memory.get_stats()};
        expect_frame_reclaimed(stats);
    }

    auto spawn(Vector3f const location,
               EntityType const type = EntityType::CapitalShip,
               Rotator3f const rotation = Rotator3f{}) -> EntityId {
        return owners.spawn(type, location, rotation);
    }

    auto ids(std::span<EntityId const> const entity_ids) const -> std::vector<EntityId> {
        return {entity_ids.begin(), entity_ids.end()};
    }

    void finish_spawning() {
        owners.publish();
        owners.refresh(query_manager);
        query_manager.reset_frame_collision_events();
    }

    auto add_static(Vector3f const min_point, Vector3f const max_point)
        -> collision::StaticGeometryIndex {
        return query_manager.add_static_collision_aabb(min_point, max_point);
    }

    void run_tick(std::span<EntityMove const> const moves) {
        query_manager.reset_frame_collision_events();
        std::vector<EntityId> candidates;
        candidates.reserve(moves.size());
        for (auto const& move : moves) {
            owners.set(move.id, move.location, move.rotation, move.alive ? 100 : 0);
            candidates.push_back(move.id);
        }
        owners.publish();
        refresh_and_detect_overlaps(candidates);
    }

    void run_tick(std::initializer_list<EntityMove> const moves) {
        run_tick(std::span<EntityMove const>{moves.begin(), moves.size()});
    }

    void run_quiet_tick() {
        query_manager.reset_frame_collision_events();
        owners.publish();
        refresh_and_detect_overlaps({});
    }

    auto get_entity_overlaps() const -> collision::EntityEntityOverlaps::ConstView {
        return query_manager.get_aabb_overlap_events().entity_entity_overlaps;
    }

    auto get_static_overlaps() const -> collision::EntityStaticOverlaps::ConstView {
        return query_manager.get_aabb_overlap_events().entity_static_overlaps;
    }

    void set_bounds(EntityType const type, Vector3f const centre, Vector3f const half_extents) {
        entity_bounds.set_centre(type, centre);
        entity_bounds.set_half_extents(type, half_extents);
    }

    CollisionAgentStorage owners;
    // Include raw duplicate overlap pairs as well as sorting scratch in the frame budget.
    GameMemory game_memory{{.root_capacity_bytes = overlap_frame_memory_capacity}};
    memory::Block frame_memory_backing{game_memory.acquire_block(
        overlap_frame_memory_capacity, FrameMemoryResource::backing_alignment)};
    FrameMemoryResource frame_memory{
        std::span<std::byte>{frame_memory_backing.data(), frame_memory_backing.size_bytes()}};
    SpatialQueryManager query_manager;
    collision::EntityAABBs entity_bounds;
};

struct OverlapEventFixture : OverlapFixture {
    OverlapEventFixture() {
        finish_spawning();
        run_tick({{moved, {15.f, 0.f, 0.f}}});
    }

    collision::StaticGeometryIndex static_index{
        add_static({{-10.f, -10.f, -10.f}}, {{10.f, 10.f, 10.f}})};
    EntityId stationary{spawn({{0.f, 0.f, 0.f}})};
    EntityId moved{spawn({{100.f, 0.f, 0.f}})};
};

void expect_empty_collision_events(collision::AABBOverlapEventsView const& events) {
    EXPECT_EQ(events.entity_entity_overlaps.num(), 0);
    EXPECT_EQ(events.entity_static_overlaps.num(), 0);
    EXPECT_TRUE(events.batches.empty());
}

void check_single_pair(collision::EntityEntityOverlaps::ConstView const overlaps,
                       EntityId const lhs,
                       EntityId const rhs) {
    EXPECT_EQ(overlaps.num(), 1) << "Exactly one overlap pair is reported";
    if (overlaps.num() == 1) {
        auto const first{lhs < rhs ? lhs : rhs};
        auto const second{lhs < rhs ? rhs : lhs};
        EXPECT_TRUE(overlaps.first_entities()[0] == first &&
                    overlaps.second_entities()[0] == second)
            << "Overlap pair is canonical";
        EXPECT_TRUE(overlaps.first_entities()[0] < overlaps.second_entities()[0])
            << "First overlap ID sorts before second";
    }
}

void check_single_static_overlap(collision::EntityStaticOverlaps::ConstView const overlaps,
                                 EntityId const entity,
                                 collision::StaticGeometryIndex const static_geometry_index) {
    EXPECT_EQ(overlaps.num(), 1) << "Exactly one static overlap is reported";
    if (overlaps.num() == 1) {
        EXPECT_TRUE(overlaps.entities()[0] == entity &&
                    overlaps.static_geometry_indices()[0] == static_geometry_index)
            << "Static overlap identity is correct";
    }
}
}

TEST(EntityAABBOverlaps, MovedEntityOverlapsStationaryEntity) {
    alignas(FrameMemoryResource::backing_alignment) std::array<std::byte, 64 * 1024>
        query_backing{};
    FrameMemoryResource query_memory{query_backing};
    FrameScratchScope query_scope{query_memory};

    OverlapFixture fixture;
    auto const stationary{
        fixture.owners.spawn(EntityType::CapitalShip, {{0.f, 0.f, 0.f}}, {}, 100, Team::Green)};
    auto const moved{fixture.spawn({{100.f, 0.f, 0.f}})};
    fixture.finish_spawning();

    std::array const handles{moved};
    fixture.run_tick({{moved, {15.f, 0.f, 0.f}}});

    check_single_pair(fixture.get_entity_overlaps(), moved, stationary);
    EXPECT_EQ(fixture.get_static_overlaps().num(), 0)
        << "A dynamic-only overlap produces no static record";

    auto owner{fixture.owners.capitals.get_view()};
    set_vector(owner.view_locations(), 0, {{300.f, 0.f, 0.f}});
    set_vector(owner.view_locations(), 1, {{315.f, 0.f, 0.f}});
    fixture.query_manager.reset_frame_collision_events();
    fixture.refresh_and_detect_overlaps(handles);
    check_single_pair(fixture.get_entity_overlaps(), moved, stationary);
    auto const& queries{fixture.query_manager};
    std::array<EntityId, 2> nearby{};
    EXPECT_EQ(queries.collect_non_team_entities_in_range(
                  {{300.f, 0.f, 0.f}}, Team::Blue, 10.f, nearby, &query_memory),
              1);
    EXPECT_EQ(nearby[0], stationary);
    EXPECT_EQ(queries.get_any_non_team_entity(Team::Blue, EntityType::CapitalShip), stationary);
    EXPECT_TRUE(!queries.get_any_non_team_entity(Team::Blue, EntityType::Turret).is_valid());
    std::array<EntityId, 2> nearby_ids{};
    EXPECT_EQ(queries.collect_entities_of_type_in_range({{300.f, 0.f, 0.f}},
                                                        EntityType::CapitalShip,
                                                        20.f,
                                                        stationary,
                                                        nearby_ids,
                                                        &query_memory),
              1);
    EXPECT_EQ(nearby_ids[0], moved);
    fixture.owners.health_table.get_view<EntityType::CapitalShip>(owner.num())
        .set_health(0, Health{0});
    fixture.owners.publish();
    fixture.owners.refresh(fixture.query_manager);
    EXPECT_EQ(queries.collect_non_team_entities_in_range(
                  {{300.f, 0.f, 0.f}}, Team::Blue, 20.f, nearby, &query_memory),
              0);
    EXPECT_TRUE(!queries.get_any_non_team_entity(Team::Blue, EntityType::CapitalShip).is_valid());
    fixture.query_manager.reset_frame_collision_events();
    fixture.refresh_and_detect_overlaps(handles);
    EXPECT_EQ(fixture.get_entity_overlaps().num(), 0)
        << "Dead entities are excluded after the spatial index is rebuilt";
}

TEST(EntityAABBOverlaps, TwoMovedEntitiesProduceOnePair) {
    OverlapFixture fixture;
    auto const first{fixture.spawn({{-100.f, 0.f, 0.f}})};
    auto const second{fixture.spawn({{100.f, 0.f, 0.f}})};
    fixture.finish_spawning();

    fixture.run_tick({{first, {-5.f, 0.f, 0.f}}, {second, {5.f, 0.f, 0.f}}});

    check_single_pair(fixture.get_entity_overlaps(), first, second);
}

TEST(EntityAABBOverlaps, SharedCellWithoutExactOverlapProducesNoPair) {
    OverlapFixture fixture{{{5.f, 5.f, 5.f}}};
    auto const stationary{fixture.spawn({{40.f, 10.f, 10.f}})};
    auto const moved{fixture.spawn({{20.f, 10.f, 10.f}})};
    fixture.finish_spawning();

    fixture.run_tick({{moved, {10.f, 10.f, 10.f}}});

    EXPECT_EQ(fixture.get_entity_overlaps().num(), 0)
        << "Broad-phase cell sharing is rejected by exact AABB testing";
    auto const cell_entities{SpatialQueryManagerTestAccess::uniform_grid(fixture.query_manager)
                                 .get_cell_entities({20, 20, 20})};
    EXPECT_TRUE(std::ranges::find(cell_entities, stationary) != cell_entities.end() &&
                std::ranges::find(cell_entities, moved) != cell_entities.end())
        << "Both entities remain in the same grid cell";
}

TEST(EntityAABBOverlaps, MultiCellOverlapProducesOnePair) {
    OverlapFixture fixture{{{120.f, 120.f, 120.f}}};
    auto const stationary{fixture.spawn({{0.f, 0.f, 0.f}})};
    auto const moved{fixture.spawn({{400.f, 0.f, 0.f}})};
    fixture.finish_spawning();

    fixture.run_tick({{moved, {10.f, 0.f, 0.f}}});

    check_single_pair(fixture.get_entity_overlaps(), moved, stationary);
}

TEST(EntityAABBOverlaps, SelfAndQuietTicksProduceNoPairs) {
    OverlapFixture fixture;
    auto const entity{fixture.spawn({{0.f, 0.f, 0.f}})};
    fixture.finish_spawning();

    fixture.run_tick({{entity, {1.f, 0.f, 0.f}}});
    EXPECT_EQ(fixture.get_entity_overlaps().num(), 0) << "Moved entity is never paired with itself";

    fixture.run_quiet_tick();
    EXPECT_EQ(fixture.get_entity_overlaps().num(), 0)
        << "A quiet tick publishes an empty overlap result";
}

TEST(EntityAABBOverlaps, RefreshSpatialIndexDoesNotDetectOrAppendEvents) {
    alignas(FrameMemoryResource::backing_alignment) std::array<std::byte, 64 * 1024>
        query_backing{};
    FrameMemoryResource query_memory{query_backing};
    FrameScratchScope query_scope{query_memory};

    OverlapFixture fixture;
    auto const entity{fixture.spawn({{0.f, 0.f, 0.f}})};
    fixture.finish_spawning();

    for (std::int32_t refresh{}; refresh < 3; ++refresh) {
        fixture.owners.refresh(fixture.query_manager);
    }

    auto const hit{fixture.query_manager.trace_closest(
        {{-20.f, 0.f, 0.f}}, {{20.f, 0.f, 0.f}}, &query_memory)};
    EXPECT_TRUE(hit.hit && hit.entity == entity)
        << "Refresh-only updates make entities available to queries";

    auto const events{fixture.query_manager.get_aabb_overlap_events()};
    SCOPED_TRACE("Refresh-only updates append no event batches");
    expect_empty_collision_events(events);
}

TEST(EntityAABBOverlaps, DetectOverlapsUsesExistingSpatialIndex) {
    OverlapFixture fixture;
    auto const candidate{fixture.spawn({{0.f, 0.f, 0.f}})};
    auto const stationary{fixture.spawn({{500.f, 0.f, 0.f}})};
    fixture.finish_spawning();

    fixture.owners.set(stationary, {{15.f, 0.f, 0.f}}, {}, 100);
    fixture.owners.publish();
    std::array const candidates{candidate};
    {
        FrameScratchScope scratch_scope{fixture.frame_memory};
        auto const overlaps{
            fixture.query_manager.detect_overlaps(candidates, &fixture.frame_memory)};
        EXPECT_EQ(overlaps.entity_entity_overlaps.num(), 0);
    }

    fixture.owners.refresh(fixture.query_manager);
    {
        FrameScratchScope scratch_scope{fixture.frame_memory};
        auto const overlaps{
            fixture.query_manager.detect_overlaps(candidates, &fixture.frame_memory)};
        check_single_pair(overlaps.entity_entity_overlaps, candidate, stationary);
    }

    auto const events{fixture.query_manager.get_aabb_overlap_events()};
    EXPECT_EQ(events.batches.size(), 2u);
    check_single_pair(events.entity_entity_overlaps, candidate, stationary);
}

TEST(EntityAABBOverlaps, MovingOutOfOverlapRemovesPairFromCurrentResult) {
    OverlapFixture fixture;
    auto const stationary{fixture.spawn({{0.f, 0.f, 0.f}})};
    auto const moved{fixture.spawn({{100.f, 0.f, 0.f}})};
    fixture.finish_spawning();

    fixture.run_tick({{moved, {15.f, 0.f, 0.f}}});
    check_single_pair(fixture.get_entity_overlaps(), moved, stationary);

    fixture.run_tick({{moved, {100.f, 0.f, 0.f}}});
    EXPECT_EQ(fixture.get_entity_overlaps().num(), 0)
        << "Separated entities are absent from the current result";
}

struct StaticOverlapCase {
    char const* name;
    float half_extent;
    Vector3f minimum;
    Vector3f maximum;
    Vector3f start;
    Vector3f end;
    bool overlaps{true};
};

class StaticEntityAABBOverlaps : public ::testing::TestWithParam<StaticOverlapCase> {};

TEST_P(StaticEntityAABBOverlaps, ReportsExactOverlapOnce) {
    auto const& test{GetParam()};
    OverlapFixture fixture{{{test.half_extent, test.half_extent, test.half_extent}}};
    auto const static_index{fixture.add_static(test.minimum, test.maximum)};
    auto const moved{fixture.spawn(test.start)};
    fixture.finish_spawning();
    fixture.run_tick({{moved, test.end}});

    if (test.overlaps) {
        check_single_static_overlap(fixture.get_static_overlaps(), moved, static_index);
    } else {
        EXPECT_EQ(fixture.get_static_overlaps().num(), 0);
    }
    EXPECT_EQ(fixture.get_entity_overlaps().num(), 0)
        << "A static-only overlap produces no dynamic pair";
}

INSTANTIATE_TEST_SUITE_P(
    Geometry,
    StaticEntityAABBOverlaps,
    ::testing::Values(StaticOverlapCase{"MovedEntityOverlapsOneStaticAABB",
                                        10.f,
                                        {-10.f, -10.f, -10.f},
                                        {10.f, 10.f, 10.f},
                                        {100.f, 0.f, 0.f},
                                        {15.f, 0.f, 0.f}},
                      StaticOverlapCase{"SharedCellWithoutExactStaticOverlapProducesNoRecord",
                                        5.f,
                                        {30.f, 5.f, 5.f},
                                        {40.f, 15.f, 15.f},
                                        {100.f, 10.f, 10.f},
                                        {10.f, 10.f, 10.f},
                                        false},
                      StaticOverlapCase{"MultiCellStaticColliderProducesOneRecord",
                                        5.f,
                                        {-120.f, -120.f, -120.f},
                                        {120.f, 120.f, 120.f},
                                        {400.f, 0.f, 0.f},
                                        {}},
                      StaticOverlapCase{"MultiCellEntityProducesOneStaticRecord",
                                        120.f,
                                        {-5.f, -5.f, -5.f},
                                        {5.f, 5.f, 5.f},
                                        {400.f, 0.f, 0.f},
                                        {10.f, 0.f, 0.f}}),
    [](auto const& test) { return test.param.name; });

TEST(EntityAABBOverlaps, MovedEntityOverlapsMultipleStaticAABBs) {
    OverlapFixture fixture;
    auto const first_static{fixture.add_static({{-20.f, -20.f, -20.f}}, {{20.f, 20.f, 20.f}})};
    auto const second_static{fixture.add_static({{-5.f, -30.f, -5.f}}, {{5.f, 30.f, 5.f}})};
    auto const moved{fixture.spawn({{100.f, 0.f, 0.f}})};
    fixture.finish_spawning();

    fixture.run_tick({{moved, {}}});

    auto const overlaps{fixture.get_static_overlaps()};
    EXPECT_EQ(overlaps.num(), 2) << "Both static overlaps are reported";
    if (overlaps.num() == 2) {
        EXPECT_TRUE(overlaps.entities()[0] == moved &&
                    overlaps.static_geometry_indices()[0] == first_static &&
                    overlaps.entities()[1] == moved &&
                    overlaps.static_geometry_indices()[1] == second_static)
            << "Static overlaps are sorted by geometry index";
    }
}

TEST(EntityAABBOverlaps, MultipleMovedEntitiesProduceDistinctStaticRecords) {
    OverlapFixture fixture;
    auto const static_index{fixture.add_static({{-20.f, -20.f, -20.f}}, {{20.f, 20.f, 20.f}})};
    auto const first{fixture.spawn({{-100.f, 0.f, 0.f}})};
    auto const second{fixture.spawn({{100.f, 0.f, 0.f}})};
    fixture.finish_spawning();

    fixture.run_tick({{first, {-10.f, 0.f, 0.f}}, {second, {10.f, 0.f, 0.f}}});

    auto const overlaps{fixture.get_static_overlaps()};
    EXPECT_EQ(overlaps.num(), 2) << "Each moved entity has its own static overlap";
    if (overlaps.num() == 2) {
        EXPECT_TRUE(overlaps.entities()[0] == first &&
                    overlaps.static_geometry_indices()[0] == static_index &&
                    overlaps.entities()[1] == second &&
                    overlaps.static_geometry_indices()[1] == static_index)
            << "Static overlap records retain both identities";
    }
}

class RotatedEntityAABBOverlaps : public ::testing::TestWithParam<bool> {};

TEST_P(RotatedEntityAABBOverlaps, RotationOnlyMovementUsesConservativeBounds) {
    OverlapFixture fixture{{{40.f, 5.f, 5.f}}, {{30.f, 0.f, 0.f}}, {{5.f, 5.f, 5.f}}};
    std::optional<collision::StaticGeometryIndex> static_index;
    if (GetParam()) {
        static_index = fixture.add_static({{-5.f, 55.f, -5.f}}, {{5.f, 65.f, 5.f}});
    }
    auto const rotated{fixture.spawn({{0.f, 0.f, 0.f}})};
    auto const stationary{fixture.spawn({{0.f, 60.f, 0.f}}, EntityType::Turret)};
    fixture.finish_spawning();

    auto const rotation{Rotator3f{0.f, 90.f, 0.f}};
    auto const expected_bounds{collision::make_entity_world_bounds(
        fixture.entity_bounds, EntityType::CapitalShip, Vector3f{}, to_quaternion(rotation))};
    EXPECT_TRUE(expected_bounds.min.y() <= 55.f && expected_bounds.max.y() >= 65.f)
        << "Rotated conservative bounds reach the stationary entity";
    fixture.run_tick({{rotated, {}, rotation}});

    check_single_pair(fixture.get_entity_overlaps(), rotated, stationary);
    if (static_index) {
        check_single_static_overlap(fixture.get_static_overlaps(), rotated, *static_index);
    }
}

INSTANTIATE_TEST_SUITE_P(Streams,
                         RotatedEntityAABBOverlaps,
                         ::testing::Bool(),
                         [](auto const& test) { return test.param ? "Both" : "DynamicOnly"; });

TEST(EntityAABBOverlaps, ManyMovedEntitiesProduceSortedUniqueResults) {
    OverlapFixture fixture{{{60.f, 60.f, 60.f}}};
    auto const large_static{
        fixture.add_static({{-450.f, -450.f, -100.f}}, {{450.f, 450.f, 100.f}})};
    for (std::int32_t index{}; index < 8; ++index) {
        auto const x{static_cast<float>((index % 4) * 160 - 240)};
        auto const y{static_cast<float>((index / 4) * 240 - 120)};
        fixture.add_static({{x - 80.f, y - 80.f, -80.f}}, {{x + 80.f, y + 80.f, 80.f}});
    }

    constexpr std::int32_t entity_count{32};
    std::array<EntityId, entity_count> moved_entities{};
    std::array<EntityMove, entity_count> moves{};
    for (std::int32_t index{}; index < entity_count; ++index) {
        auto const x{static_cast<float>((index % 8) * 100 - 350)};
        auto const y{static_cast<float>((index / 8) * 100 - 150)};
        fixture.spawn({{x, y, 0.f}});
        moved_entities[index] = fixture.spawn({{x, y, 500.f}});
        moves[index] = {moved_entities[index], {x, y, 0.f}};
    }
    fixture.finish_spawning();
    fixture.run_tick(moves);

    auto const entity_overlaps{fixture.get_entity_overlaps()};
    EXPECT_TRUE(entity_overlaps.num() >= entity_count) << "Large query produces dynamic overlaps";
    auto const entity_overlap_count{entity_overlaps.num()};
    auto const first_entities{entity_overlaps.first_entities()};
    auto const second_entities{entity_overlaps.second_entities()};
    for (std::uint32_t index{}; index < entity_overlap_count; ++index) {
        EXPECT_TRUE(entity_is_alive(fixture.owners, first_entities[index]) &&
                    entity_is_alive(fixture.owners, second_entities[index]))
            << "Dynamic overlap handles remain valid";
        EXPECT_TRUE(first_entities[index] < second_entities[index])
            << "Dynamic overlap is canonical";
        if (index > 0) {
            auto const previous_first{first_entities[index - 1]};
            auto const previous_second{second_entities[index - 1]};
            auto const current_first{first_entities[index]};
            auto const current_second{second_entities[index]};
            EXPECT_TRUE(previous_first < current_first ||
                        (previous_first == current_first && previous_second < current_second))
                << "Dynamic overlaps are sorted and unique";
        }
    }

    auto const static_overlaps{fixture.get_static_overlaps()};
    EXPECT_TRUE(static_overlaps.num() >= entity_count)
        << "Every moved entity overlaps the large static AABB";
    auto const static_overlap_count{static_overlaps.num()};
    auto const static_geometry_count{fixture.query_manager.get_static_collision_bounds().num()};
    auto const static_entities{static_overlaps.entities()};
    auto const static_indices{static_overlaps.static_geometry_indices()};
    for (std::uint32_t index{}; index < static_overlap_count; ++index) {
        EXPECT_TRUE(entity_is_alive(fixture.owners, static_entities[index]))
            << "Static overlap entity remains valid";
        EXPECT_TRUE(static_indices[index].value() < static_geometry_count)
            << "Static overlap index remains valid";
        if (index > 0) {
            auto const previous_entity{static_entities[index - 1]};
            auto const previous_static{static_indices[index - 1]};
            auto const current_entity{static_entities[index]};
            auto const current_static{static_indices[index]};
            EXPECT_TRUE(previous_entity < current_entity ||
                        (previous_entity == current_entity && previous_static < current_static))
                << "Static overlaps are sorted and unique";
        }
    }
    for (auto const moved : moved_entities) {
        bool found_large_static{};
        for (std::uint32_t index{}; index < static_overlap_count; ++index) {
            found_large_static = found_large_static || (static_entities[index] == moved &&
                                                        static_indices[index] == large_static);
        }
        EXPECT_TRUE(found_large_static) << "Moved entity retains its large-static identity";
    }

    auto const expected_first{fixture.ids(entity_overlaps.first_entities())};
    auto const expected_second{fixture.ids(entity_overlaps.second_entities())};
    auto const expected_static_entities{fixture.ids(static_overlaps.entities())};
    std::vector<collision::StaticGeometryIndex> const expected_static_indices{
        static_overlaps.static_geometry_indices().begin(),
        static_overlaps.static_geometry_indices().end()};
    std::vector<EntityId> repeated_candidates;
    for (std::uint32_t repeat{}; repeat < 4; ++repeat) {
        repeated_candidates.insert(
            repeated_candidates.end(), moved_entities.rbegin(), moved_entities.rend());
    }
    fixture.query_manager.reset_frame_collision_events();
    fixture.refresh_and_detect_overlaps(repeated_candidates);
    auto const repeated_entities{fixture.get_entity_overlaps()};
    auto const repeated_static{fixture.get_static_overlaps()};
    EXPECT_EQ(fixture.ids(repeated_entities.first_entities()), expected_first);
    EXPECT_EQ(fixture.ids(repeated_entities.second_entities()), expected_second);
    EXPECT_EQ(fixture.ids(repeated_static.entities()), expected_static_entities);
    EXPECT_EQ(std::vector<collision::StaticGeometryIndex>(
                  repeated_static.static_geometry_indices().begin(),
                  repeated_static.static_geometry_indices().end()),
              expected_static_indices);
}

TEST(EntityAABBOverlaps, EventsMirrorAuthoritativeResults) {
    OverlapEventFixture fixture;

    check_single_pair(fixture.get_entity_overlaps(), fixture.moved, fixture.stationary);
    check_single_static_overlap(fixture.get_static_overlaps(), fixture.moved, fixture.static_index);

    auto const events{fixture.query_manager.get_aabb_overlap_events()};
    EXPECT_EQ(static_cast<std::int32_t>(events.batches.size()), 1)
        << "The collision pass records one event batch";
    if (events.batches.size() == 1) {
        auto const batch{events.get_batch(0)};
        check_single_pair(batch.entity_entity_overlaps, fixture.moved, fixture.stationary);
        check_single_static_overlap(
            batch.entity_static_overlaps, fixture.moved, fixture.static_index);
    }
}

TEST(EntityAABBOverlaps, ReinitialiseClearsResultsAndAllowsSubsequentUpdate) {
    OverlapEventFixture fixture;

    fixture.query_manager.initialise({{40, 40, 40}, {{50.f, 50.f, 50.f}}}, fixture.entity_bounds);

    auto const cleared_events{fixture.query_manager.get_aabb_overlap_events()};
    EXPECT_EQ(cleared_events.entity_entity_overlaps.num(), 0)
        << "Reinitialisation clears dynamic overlap events";
    EXPECT_EQ(cleared_events.entity_static_overlaps.num(), 0)
        << "Reinitialisation clears static overlap events";

    std::array const handles{fixture.moved};
    fixture.refresh_and_detect_overlaps(handles);
    check_single_pair(fixture.get_entity_overlaps(), fixture.moved, fixture.stationary);
    check_single_static_overlap(fixture.get_static_overlaps(), fixture.moved, fixture.static_index);
}

TEST(EntityAABBOverlaps, FrameEventResetRetainsStorageAndPassesAppend) {
    OverlapEventFixture fixture;
    std::array const handles{fixture.moved};

    auto const first_events{fixture.query_manager.get_aabb_overlap_events()};
    check_single_pair(first_events.entity_entity_overlaps, fixture.moved, fixture.stationary);
    check_single_static_overlap(
        first_events.entity_static_overlaps, fixture.moved, fixture.static_index);
    auto const* const entity_storage{first_events.entity_entity_overlaps.first_entities().data()};
    auto const* const static_storage{first_events.entity_static_overlaps.entities().data()};

    fixture.query_manager.reset_frame_collision_events();

    auto const reset_events{fixture.query_manager.get_aabb_overlap_events()};
    SCOPED_TRACE("Frame reset clears event batches");
    expect_empty_collision_events(reset_events);
    EXPECT_TRUE(reset_events.entity_entity_overlaps.first_entities().data() == entity_storage)
        << "Dynamic event storage is retained across reset";
    EXPECT_TRUE(reset_events.entity_static_overlaps.entities().data() == static_storage)
        << "Static event storage is retained across reset";

    fixture.refresh_and_detect_overlaps(handles);
    auto const recaptured_events{fixture.query_manager.get_aabb_overlap_events()};
    check_single_pair(recaptured_events.entity_entity_overlaps, fixture.moved, fixture.stationary);
    check_single_static_overlap(
        recaptured_events.entity_static_overlaps, fixture.moved, fixture.static_index);
    EXPECT_TRUE(recaptured_events.entity_entity_overlaps.first_entities().data() == entity_storage)
        << "Dynamic event storage is reused after recapture";
    EXPECT_TRUE(recaptured_events.entity_static_overlaps.entities().data() == static_storage)
        << "Static event storage is reused after recapture";

    fixture.refresh_and_detect_overlaps(handles);
    auto const appended_events{fixture.query_manager.get_aabb_overlap_events()};
    EXPECT_EQ(appended_events.entity_entity_overlaps.num(), 2)
        << "Collision passes append dynamic events within a frame";
    EXPECT_EQ(appended_events.entity_static_overlaps.num(), 2)
        << "Collision passes append static events within a frame";
    EXPECT_EQ(static_cast<std::int32_t>(appended_events.batches.size()), 2)
        << "Collision passes retain separate batch metadata";
    fixture.query_manager.reset_frame_collision_events();
    auto const next_frame_events{fixture.query_manager.get_aabb_overlap_events()};
    SCOPED_TRACE("The next frame starts without event batches");
    expect_empty_collision_events(next_frame_events);
}

TEST(EntityAABBOverlaps, NullDeadAndRetiredCandidatesAreIgnored) {
    OverlapFixture fixture;
    auto const static_index{fixture.add_static({{-20.f, -20.f, -20.f}}, {{20.f, 20.f, 20.f}})};
    auto const live{fixture.spawn({{0.f, 0.f, 0.f}})};
    auto const removed{fixture.spawn({{100.f, 0.f, 0.f}})};
    fixture.finish_spawning();

    auto const removed_id{removed};
    fixture.run_tick({{removed, {10.f, 0.f, 0.f}, {}, false}});
    EXPECT_TRUE(observe_entity(fixture.owners, removed).has_value() &&
                !observe_live_entity(fixture.owners, removed).has_value())
        << "Moved-and-dead entity remains structurally present until removal";
    EXPECT_EQ(fixture.get_entity_overlaps().num(), 0) << "Dead overlap candidate produces no pair";
    EXPECT_EQ(fixture.get_static_overlaps().num(), 0)
        << "Dead overlap candidate produces no static overlap";

    fixture.owners.remove(removed);
    auto const replacement{fixture.spawn({{10.f, 0.f, 0.f}})};
    fixture.owners.publish();
    EXPECT_FALSE(observe_entity(fixture.owners, removed).has_value())
        << "Removed ID no longer resolves after publication";

    std::array const overlap_candidates{EntityId{}, removed_id, live};
    fixture.refresh_and_detect_overlaps(std::span<EntityId const>{
        overlap_candidates.data(),
        static_cast<std::size_t>(static_cast<std::int32_t>(overlap_candidates.size()))});

    check_single_pair(fixture.get_entity_overlaps(), live, replacement);
    check_single_static_overlap(fixture.get_static_overlaps(), live, static_index);
}

} // namespace tests
