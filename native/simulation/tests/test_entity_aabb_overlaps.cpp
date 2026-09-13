#include <sandbox/simulation/entities/TestEntityRegistry.h>
#include <sandbox/simulation/simulation/SpatialQueryManager.h>
#include "support/simulation_test_support.h"

namespace {
struct FOverlapFixture {
    explicit FOverlapFixture(
        ml::simulation::Vector3f const capital_half_extents = {{10.f, 10.f, 10.f}},
        ml::simulation::Vector3f const capital_centre = ml::simulation::Vector3f{},
        ml::simulation::Vector3f const turret_half_extents = {{10.f, 10.f, 10.f}})
        : query_manager{registry} {
        auto const type_count{ml::simulation::collision::EntityAABBs::num()};
        for (std::int32_t type_index{}; type_index < type_count; ++type_index) {
            set_bounds(type_index,
                       ml::simulation::Vector3f{},
                       ml::simulation::Vector3f{{10.f, 10.f, 10.f}});
        }
        set_bounds(ml::simulation::collision::EntityAABBs::capital_ship_index,
                   capital_centre,
                   capital_half_extents);
        set_bounds(ml::simulation::collision::EntityAABBs::static_turret_index,
                   ml::simulation::Vector3f{},
                   turret_half_extents);

        query_manager.initialise({40, 40, 40}, {{50.f, 50.f, 50.f}}, entity_bounds);
    }

    auto spawn(ml::simulation::Vector3f const location,
               ml::simulation::EntityType const type = ml::simulation::EntityType::CapitalShip,
               ml::simulation::Rotator3f const rotation = ml::simulation::Rotator3f{})
        -> FRegistryEntityHandle {
        FTestEntityRegistry::EntityData data;
        data.add_defaulted(1);
        data.locations.set(0, location);
        data.rotations.set(0, rotation);
        data.healths[0] = 100;
        data.teams[0] = ml::simulation::Team::Blue;
        data.entity_types[0] = type;
        data.alive[0] = 1;
        return registry.add_entities(data.get_const_view()).get_handle(0);
    }

    void finish_spawning() {
        registry.commit_updates();
        query_manager.update(current_tick);
        registry.end_tick();
        query_manager.get_collision_system().reset_frame_events();
    }

    auto add_static(ml::simulation::Vector3f const min_point,
                    ml::simulation::Vector3f const max_point) -> std::int32_t {
        return query_manager.get_collision_system().get_uniform_grid().add_static_aabb(min_point,
                                                                                       max_point);
    }

    void run_tick(std::span<FRegistryEntityHandle const> const handles,
                  std::span<ml::simulation::Vector3f const> const locations,
                  std::span<ml::simulation::Rotator3f const> const rotations,
                  std::span<std::uint8_t const> const alive = {}) {
        assert(static_cast<std::int32_t>(handles.size()) ==
               static_cast<std::int32_t>(locations.size()));
        assert(static_cast<std::int32_t>(handles.size()) ==
               static_cast<std::int32_t>(rotations.size()));
        assert(alive.empty() || static_cast<std::int32_t>(handles.size()) ==
                                    static_cast<std::int32_t>(alive.size()));

        query_manager.get_collision_system().reset_frame_events();
        registry.begin_tick();

        FTestEntityRegistry::EntityData updates;
        EntityDeathInfo deaths;
        auto const& current{registry.get_entity_data()};
        auto const count{static_cast<std::int32_t>(handles.size())};
        for (std::int32_t index{}; index < count; ++index) {
            auto const handle{handles[index]};
            auto const entity_alive{alive.empty() ? std::uint8_t{1} : alive[index]};
            updates.add_defaulted(1);
            updates.copy_element(index, current, handle.index);
            updates.locations.set(index, locations[index]);
            updates.rotations.set(index, rotations[index]);
            updates.alive[index] = entity_alive;
            if (entity_alive == 0) {
                deaths.add(ml::simulation::DeathReason::Unknown, handle, {});
            }
        }

        registry.queue_entity_updates(
            {std::span<FRegistryEntityHandle const>{
                 handles.data(),
                 static_cast<std::size_t>(static_cast<std::int32_t>(handles.size()))},
             updates.get_const_view()},
            deaths);
        registry.commit_updates();
        query_manager.update(++current_tick);
        tick_is_open = true;
    }

    void run_quiet_tick() {
        end_tick();
        query_manager.get_collision_system().reset_frame_events();
        registry.begin_tick();
        registry.commit_updates();
        query_manager.update(++current_tick);
        tick_is_open = true;
    }

    void end_tick() {
        if (tick_is_open) {
            registry.end_tick();
            tick_is_open = false;
        }
    }

    auto get_entity_overlaps() const -> ml::ioj::FEntityEntityOverlaps::ConstView {
        return query_manager.get_collision_system()
            .get_aabb_overlap_events()
            .entity_entity_overlaps;
    }

    auto get_static_overlaps() const -> ml::ioj::FEntityStaticOverlaps::ConstView {
        return query_manager.get_collision_system()
            .get_aabb_overlap_events()
            .entity_static_overlaps;
    }

    void set_bounds(std::int32_t const type_index,
                    ml::simulation::Vector3f const centre,
                    ml::simulation::Vector3f const half_extents) {
        entity_bounds.centre_xs[type_index] = centre.X;
        entity_bounds.centre_ys[type_index] = centre.Y;
        entity_bounds.centre_zs[type_index] = centre.Z;
        entity_bounds.half_extent_xs[type_index] = half_extents.X;
        entity_bounds.half_extent_ys[type_index] = half_extents.Y;
        entity_bounds.half_extent_zs[type_index] = half_extents.Z;
    }

    FTestEntityRegistry registry;
    ml::FSpatialQueryManager query_manager;
    ml::simulation::collision::EntityAABBs entity_bounds;
    std::uint64_t current_tick{};
    bool tick_is_open{};
};

void check_single_pair(ml::ioj::FEntityEntityOverlaps::ConstView const overlaps,
                       FRegistryEntityHandle const lhs,
                       FRegistryEntityHandle const rhs) {
    ml::simulation_tests::expect_equal(overlaps.num(), 1, "Exactly one overlap pair is reported");
    if (overlaps.num() == 1) {
        auto const first{lhs < rhs ? lhs : rhs};
        auto const second{lhs < rhs ? rhs : lhs};
        ml::simulation_tests::expect_true(overlaps.first_entities[0] == first &&
                                              overlaps.second_entities[0] == second,
                                          "Overlap pair is canonical");
        ml::simulation_tests::expect_true(overlaps.first_entities[0] < overlaps.second_entities[0],
                                          "First overlap handle sorts before second");
    }
}

void check_single_static_overlap(ml::ioj::FEntityStaticOverlaps::ConstView const overlaps,
                                 FRegistryEntityHandle const entity,
                                 std::int32_t const static_geometry_index) {
    ml::simulation_tests::expect_equal(overlaps.num(), 1, "Exactly one static overlap is reported");
    if (overlaps.num() == 1) {
        ml::simulation_tests::expect_true(overlaps.entities[0] == entity &&
                                              overlaps.static_geometry_indices[0] ==
                                                  static_geometry_index,
                                          "Static overlap identity is correct");
    }
}
}

TEST(EntityAABBOverlaps, MovedEntityOverlapsStationaryEntity) {

    FOverlapFixture fixture;
    auto const stationary{fixture.spawn({{0.f, 0.f, 0.f}})};
    auto const moved{fixture.spawn({{100.f, 0.f, 0.f}})};
    fixture.finish_spawning();

    std::array const handles{moved};
    std::array const locations{ml::simulation::Vector3f{{15.f, 0.f, 0.f}}};
    std::array const rotations{ml::simulation::Rotator3f{}};
    fixture.run_tick(handles, locations, rotations);

    check_single_pair(fixture.get_entity_overlaps(), moved, stationary);
    ml::simulation_tests::expect_equal(
        fixture.get_static_overlaps().num(), 0, "A dynamic-only overlap produces no static record");
}

TEST(EntityAABBOverlaps, TwoMovedEntitiesProduceOnePair) {

    FOverlapFixture fixture;
    auto const first{fixture.spawn({{-100.f, 0.f, 0.f}})};
    auto const second{fixture.spawn({{100.f, 0.f, 0.f}})};
    fixture.finish_spawning();

    std::array const handles{first, second};
    std::array const locations{ml::simulation::Vector3f{{-5.f, 0.f, 0.f}},
                               ml::simulation::Vector3f{{5.f, 0.f, 0.f}}};
    std::array const rotations{ml::simulation::Rotator3f{}, ml::simulation::Rotator3f{}};
    fixture.run_tick(handles, locations, rotations);

    check_single_pair(fixture.get_entity_overlaps(), first, second);
}

TEST(EntityAABBOverlaps, SharedCellWithoutExactOverlapProducesNoPair) {

    FOverlapFixture fixture{{{5.f, 5.f, 5.f}}};
    auto const stationary{fixture.spawn({{40.f, 10.f, 10.f}})};
    auto const moved{fixture.spawn({{20.f, 10.f, 10.f}})};
    fixture.finish_spawning();

    std::array const handles{moved};
    std::array const locations{ml::simulation::Vector3f{{10.f, 10.f, 10.f}}};
    std::array const rotations{ml::simulation::Rotator3f{}};
    fixture.run_tick(handles, locations, rotations);

    ml::simulation_tests::expect_equal(
        fixture.get_entity_overlaps().num(),
        0,
        "Broad-phase cell sharing is rejected by exact AABB testing");
    auto const cell_entities{
        fixture.query_manager.get_collision_system().get_uniform_grid().get_cell_entities(
            {20, 20, 20})};
    ml::simulation_tests::expect_true(
        std::ranges::find(cell_entities, stationary) != cell_entities.end() &&
            std::ranges::find(cell_entities, moved) != cell_entities.end(),
        "Both entities remain in the same grid cell");
}

TEST(EntityAABBOverlaps, MultiCellOverlapProducesOnePair) {

    FOverlapFixture fixture{{{120.f, 120.f, 120.f}}};
    auto const stationary{fixture.spawn({{0.f, 0.f, 0.f}})};
    auto const moved{fixture.spawn({{400.f, 0.f, 0.f}})};
    fixture.finish_spawning();

    std::array const handles{moved};
    std::array const locations{ml::simulation::Vector3f{{10.f, 0.f, 0.f}}};
    std::array const rotations{ml::simulation::Rotator3f{}};
    fixture.run_tick(handles, locations, rotations);

    check_single_pair(fixture.get_entity_overlaps(), moved, stationary);
}

TEST(EntityAABBOverlaps, SelfAndQuietTicksProduceNoPairs) {

    FOverlapFixture fixture;
    auto const entity{fixture.spawn({{0.f, 0.f, 0.f}})};
    fixture.finish_spawning();

    std::array const handles{entity};
    std::array const locations{ml::simulation::Vector3f{{1.f, 0.f, 0.f}}};
    std::array const rotations{ml::simulation::Rotator3f{}};
    fixture.run_tick(handles, locations, rotations);
    ml::simulation_tests::expect_equal(
        fixture.get_entity_overlaps().num(), 0, "Moved entity is never paired with itself");

    fixture.run_quiet_tick();
    ml::simulation_tests::expect_equal(
        fixture.get_entity_overlaps().num(), 0, "A quiet tick publishes an empty overlap result");
}

TEST(EntityAABBOverlaps, RotatedConservativeWorldBoundsUseExistingBoundsRules) {

    FOverlapFixture fixture{{{40.f, 5.f, 5.f}}, {{30.f, 0.f, 0.f}}, {{5.f, 5.f, 5.f}}};
    auto const rotated{fixture.spawn({{0.f, 0.f, 0.f}})};
    auto const stationary{fixture.spawn({{0.f, 60.f, 0.f}}, ml::simulation::EntityType::Turret)};
    fixture.finish_spawning();

    auto const rotation{ml::simulation::Rotator3f{0.f, 90.f, 0.f}};
    auto const expected_bounds{ml::simulation::collision::make_entity_world_bounds(
        fixture.entity_bounds,
        ml::simulation::collision::EntityAABBs::capital_ship_index,
        ml::simulation::Vector3f{},
        ml::simulation::to_quaternion(rotation))};
    ml::simulation_tests::expect_true(expected_bounds.min.Y <= 55.f &&
                                          expected_bounds.max.Y >= 65.f,
                                      "Rotated conservative bounds reach the stationary entity");

    std::array const handles{rotated};
    std::array const locations{ml::simulation::Vector3f{}};
    std::array const rotations{rotation};
    fixture.run_tick(handles, locations, rotations);

    check_single_pair(fixture.get_entity_overlaps(), rotated, stationary);
}

TEST(EntityAABBOverlaps, MovingOutOfOverlapRemovesPairFromCurrentResult) {

    FOverlapFixture fixture;
    auto const stationary{fixture.spawn({{0.f, 0.f, 0.f}})};
    auto const moved{fixture.spawn({{100.f, 0.f, 0.f}})};
    fixture.finish_spawning();

    std::array const handles{moved};
    std::array const rotations{ml::simulation::Rotator3f{}};
    std::array const overlapping_location{ml::simulation::Vector3f{{15.f, 0.f, 0.f}}};
    fixture.run_tick(handles, overlapping_location, rotations);
    check_single_pair(fixture.get_entity_overlaps(), moved, stationary);

    fixture.end_tick();
    std::array const separated_location{ml::simulation::Vector3f{{100.f, 0.f, 0.f}}};
    fixture.run_tick(handles, separated_location, rotations);
    ml::simulation_tests::expect_equal(fixture.get_entity_overlaps().num(),
                                       0,
                                       "Separated entities are absent from the current result");
}

TEST(EntityAABBOverlaps, MovedEntityOverlapsOneStaticAABB) {

    FOverlapFixture fixture;
    auto const static_index{fixture.add_static({{-10.f, -10.f, -10.f}}, {{10.f, 10.f, 10.f}})};
    auto const moved{fixture.spawn({{100.f, 0.f, 0.f}})};
    fixture.finish_spawning();

    std::array const handles{moved};
    std::array const locations{ml::simulation::Vector3f{{15.f, 0.f, 0.f}}};
    std::array const rotations{ml::simulation::Rotator3f{}};
    fixture.run_tick(handles, locations, rotations);

    check_single_static_overlap(fixture.get_static_overlaps(), moved, static_index);
    ml::simulation_tests::expect_equal(
        fixture.get_entity_overlaps().num(), 0, "A static-only overlap produces no dynamic pair");
}

TEST(EntityAABBOverlaps, MovedEntityOverlapsMultipleStaticAABBs) {

    FOverlapFixture fixture;
    auto const first_static{fixture.add_static({{-20.f, -20.f, -20.f}}, {{20.f, 20.f, 20.f}})};
    auto const second_static{fixture.add_static({{-5.f, -30.f, -5.f}}, {{5.f, 30.f, 5.f}})};
    auto const moved{fixture.spawn({{100.f, 0.f, 0.f}})};
    fixture.finish_spawning();

    std::array const handles{moved};
    std::array const locations{ml::simulation::Vector3f{}};
    std::array const rotations{ml::simulation::Rotator3f{}};
    fixture.run_tick(handles, locations, rotations);

    auto const overlaps{fixture.get_static_overlaps()};
    ml::simulation_tests::expect_equal(overlaps.num(), 2, "Both static overlaps are reported");
    if (overlaps.num() == 2) {
        ml::simulation_tests::expect_true(overlaps.entities[0] == moved &&
                                              overlaps.static_geometry_indices[0] == first_static &&
                                              overlaps.entities[1] == moved &&
                                              overlaps.static_geometry_indices[1] == second_static,
                                          "Static overlaps are sorted by geometry index");
    }
}

TEST(EntityAABBOverlaps, SharedCellWithoutExactStaticOverlapProducesNoRecord) {

    FOverlapFixture fixture{{{5.f, 5.f, 5.f}}};
    fixture.add_static({{30.f, 5.f, 5.f}}, {{40.f, 15.f, 15.f}});
    auto const moved{fixture.spawn({{100.f, 10.f, 10.f}})};
    fixture.finish_spawning();

    std::array const handles{moved};
    std::array const locations{ml::simulation::Vector3f{{10.f, 10.f, 10.f}}};
    std::array const rotations{ml::simulation::Rotator3f{}};
    fixture.run_tick(handles, locations, rotations);

    ml::simulation_tests::expect_equal(fixture.get_static_overlaps().num(),
                                       0,
                                       "Static broad-phase cell sharing requires exact overlap");
}

TEST(EntityAABBOverlaps, MultiCellStaticColliderProducesOneRecord) {

    FOverlapFixture fixture{{{5.f, 5.f, 5.f}}};
    auto const static_index{
        fixture.add_static({{-120.f, -120.f, -120.f}}, {{120.f, 120.f, 120.f}})};
    auto const moved{fixture.spawn({{400.f, 0.f, 0.f}})};
    fixture.finish_spawning();

    std::array const handles{moved};
    std::array const locations{ml::simulation::Vector3f{}};
    std::array const rotations{ml::simulation::Rotator3f{}};
    fixture.run_tick(handles, locations, rotations);

    check_single_static_overlap(fixture.get_static_overlaps(), moved, static_index);
}

TEST(EntityAABBOverlaps, MultiCellEntityProducesOneStaticRecord) {

    FOverlapFixture fixture{{{120.f, 120.f, 120.f}}};
    auto const static_index{fixture.add_static({{-5.f, -5.f, -5.f}}, {{5.f, 5.f, 5.f}})};
    auto const moved{fixture.spawn({{400.f, 0.f, 0.f}})};
    fixture.finish_spawning();

    std::array const handles{moved};
    std::array const locations{ml::simulation::Vector3f{{10.f, 0.f, 0.f}}};
    std::array const rotations{ml::simulation::Rotator3f{}};
    fixture.run_tick(handles, locations, rotations);

    check_single_static_overlap(fixture.get_static_overlaps(), moved, static_index);
}

TEST(EntityAABBOverlaps, MultipleMovedEntitiesProduceDistinctStaticRecords) {

    FOverlapFixture fixture;
    auto const static_index{fixture.add_static({{-20.f, -20.f, -20.f}}, {{20.f, 20.f, 20.f}})};
    auto const first{fixture.spawn({{-100.f, 0.f, 0.f}})};
    auto const second{fixture.spawn({{100.f, 0.f, 0.f}})};
    fixture.finish_spawning();

    std::array const handles{first, second};
    std::array const locations{ml::simulation::Vector3f{{-10.f, 0.f, 0.f}},
                               ml::simulation::Vector3f{{10.f, 0.f, 0.f}}};
    std::array const rotations{ml::simulation::Rotator3f{}, ml::simulation::Rotator3f{}};
    fixture.run_tick(handles, locations, rotations);

    auto const overlaps{fixture.get_static_overlaps()};
    ml::simulation_tests::expect_equal(
        overlaps.num(), 2, "Each moved entity has its own static overlap");
    if (overlaps.num() == 2) {
        ml::simulation_tests::expect_true(overlaps.entities[0] == first &&
                                              overlaps.static_geometry_indices[0] == static_index &&
                                              overlaps.entities[1] == second &&
                                              overlaps.static_geometry_indices[1] == static_index,
                                          "Static overlap records retain both identities");
    }
}

TEST(EntityAABBOverlaps, MovedEntityProducesDynamicAndStaticOverlapsTogether) {

    FOverlapFixture fixture;
    auto const static_index{fixture.add_static({{-10.f, -10.f, -10.f}}, {{10.f, 10.f, 10.f}})};
    auto const stationary{fixture.spawn({{0.f, 0.f, 0.f}})};
    auto const moved{fixture.spawn({{100.f, 0.f, 0.f}})};
    fixture.finish_spawning();

    std::array const handles{moved};
    std::array const locations{ml::simulation::Vector3f{{15.f, 0.f, 0.f}}};
    std::array const rotations{ml::simulation::Rotator3f{}};
    fixture.run_tick(handles, locations, rotations);

    check_single_pair(fixture.get_entity_overlaps(), moved, stationary);
    check_single_static_overlap(fixture.get_static_overlaps(), moved, static_index);
}

TEST(EntityAABBOverlaps, RotationOnlyMovementUsesConservativeBoundsForBothStreams) {

    FOverlapFixture fixture{{{40.f, 5.f, 5.f}}, {{30.f, 0.f, 0.f}}, {{5.f, 5.f, 5.f}}};
    auto const static_index{fixture.add_static({{-5.f, 55.f, -5.f}}, {{5.f, 65.f, 5.f}})};
    auto const rotated{fixture.spawn({{0.f, 0.f, 0.f}})};
    auto const stationary{fixture.spawn({{0.f, 60.f, 0.f}}, ml::simulation::EntityType::Turret)};
    fixture.finish_spawning();

    std::array const handles{rotated};
    std::array const locations{ml::simulation::Vector3f{}};
    std::array const rotations{ml::simulation::Rotator3f{0.f, 90.f, 0.f}};
    fixture.run_tick(handles, locations, rotations);

    check_single_pair(fixture.get_entity_overlaps(), rotated, stationary);
    check_single_static_overlap(fixture.get_static_overlaps(), rotated, static_index);
}

TEST(EntityAABBOverlaps, ManyMovedEntitiesProduceSortedUniqueResults) {

    FOverlapFixture fixture{{{60.f, 60.f, 60.f}}};
    auto const large_static{
        fixture.add_static({{-450.f, -450.f, -100.f}}, {{450.f, 450.f, 100.f}})};
    for (std::int32_t index{}; index < 8; ++index) {
        auto const x{static_cast<float>((index % 4) * 160 - 240)};
        auto const y{static_cast<float>((index / 4) * 240 - 120)};
        fixture.add_static({{x - 80.f, y - 80.f, -80.f}}, {{x + 80.f, y + 80.f, 80.f}});
    }

    std::vector<FRegistryEntityHandle> moved_entities{};
    std::vector<ml::simulation::Vector3f> moved_locations{};
    std::vector<ml::simulation::Rotator3f> moved_rotations{};
    constexpr std::int32_t entity_count{32};
    moved_entities.reserve(entity_count);
    moved_locations.reserve(entity_count);
    moved_rotations.reserve(entity_count);
    for (std::int32_t index{}; index < entity_count; ++index) {
        auto const x{static_cast<float>((index % 8) * 100 - 350)};
        auto const y{static_cast<float>((index / 8) * 100 - 150)};
        fixture.spawn({{x, y, 0.f}});
        moved_entities.push_back(fixture.spawn({{x, y, 500.f}}));
        moved_locations.push_back({{x, y, 0.f}});
        moved_rotations.push_back(ml::simulation::Rotator3f{});
    }
    fixture.finish_spawning();
    fixture.run_tick(moved_entities, moved_locations, moved_rotations);

    auto const entity_overlaps{fixture.get_entity_overlaps()};
    ml::simulation_tests::expect_true(entity_overlaps.num() >= entity_count,
                                      "Large query produces dynamic overlaps");
    for (std::int32_t index{}; index < entity_overlaps.num(); ++index) {
        ml::simulation_tests::expect_true(
            fixture.registry.is_valid_alive(entity_overlaps.first_entities[index]) &&
                fixture.registry.is_valid_alive(entity_overlaps.second_entities[index]),
            "Dynamic overlap handles remain valid");
        ml::simulation_tests::expect_true(entity_overlaps.first_entities[index] <
                                              entity_overlaps.second_entities[index],
                                          "Dynamic overlap is canonical");
        if (index > 0) {
            auto const previous_first{entity_overlaps.first_entities[index - 1]};
            auto const previous_second{entity_overlaps.second_entities[index - 1]};
            auto const current_first{entity_overlaps.first_entities[index]};
            auto const current_second{entity_overlaps.second_entities[index]};
            ml::simulation_tests::expect_true(
                previous_first < current_first ||
                    (previous_first == current_first && previous_second < current_second),
                "Dynamic overlaps are sorted and unique");
        }
    }

    auto const static_overlaps{fixture.get_static_overlaps()};
    ml::simulation_tests::expect_true(static_overlaps.num() >= entity_count,
                                      "Every moved entity overlaps the large static AABB");
    for (std::int32_t index{}; index < static_overlaps.num(); ++index) {
        ml::simulation_tests::expect_true(
            fixture.registry.is_valid_alive(static_overlaps.entities[index]),
            "Static overlap entity remains valid");
        auto const static_geometry_count{fixture.query_manager.get_collision_system()
                                             .get_uniform_grid()
                                             .get_static_aabbs()
                                             .num()};
        ml::simulation_tests::expect_true(static_overlaps.static_geometry_indices[index] >= 0 &&
                                              static_overlaps.static_geometry_indices[index] <
                                                  static_geometry_count,
                                          "Static overlap index remains valid");
        if (index > 0) {
            auto const previous_entity{static_overlaps.entities[index - 1]};
            auto const previous_static{static_overlaps.static_geometry_indices[index - 1]};
            auto const current_entity{static_overlaps.entities[index]};
            auto const current_static{static_overlaps.static_geometry_indices[index]};
            ml::simulation_tests::expect_true(
                previous_entity < current_entity ||
                    (previous_entity == current_entity && previous_static < current_static),
                "Static overlaps are sorted and unique");
        }
    }
    for (auto const moved : moved_entities) {
        bool found_large_static{};
        for (std::int32_t index{}; index < static_overlaps.num(); ++index) {
            found_large_static = found_large_static ||
                                 (static_overlaps.entities[index] == moved &&
                                  static_overlaps.static_geometry_indices[index] == large_static);
        }
        ml::simulation_tests::expect_true(found_large_static,
                                          "Moved entity retains its large-static identity");
    }
}

TEST(EntityAABBOverlaps, EventsMirrorAuthoritativeResults) {

    FOverlapFixture fixture;
    auto const static_index{fixture.add_static({{-10.f, -10.f, -10.f}}, {{10.f, 10.f, 10.f}})};
    auto const stationary{fixture.spawn({{0.f, 0.f, 0.f}})};
    auto const moved{fixture.spawn({{100.f, 0.f, 0.f}})};
    fixture.finish_spawning();

    std::array const handles{moved};
    std::array const locations{ml::simulation::Vector3f{{15.f, 0.f, 0.f}}};
    std::array const rotations{ml::simulation::Rotator3f{}};
    fixture.run_tick(handles, locations, rotations);

    auto const& collision_system{fixture.query_manager.get_collision_system()};
    check_single_pair(collision_system.get_entity_entity_overlaps(), moved, stationary);
    check_single_static_overlap(collision_system.get_entity_static_overlaps(), moved, static_index);
    check_single_pair(fixture.get_entity_overlaps(), moved, stationary);
    check_single_static_overlap(fixture.get_static_overlaps(), moved, static_index);

    auto const events{collision_system.get_aabb_overlap_events()};
    ml::simulation_tests::expect_equal(static_cast<std::int32_t>(events.batches.size()),
                                       1,
                                       "The collision pass records one event batch");
    if (events.batches.size() == 1) {
        auto const batch{events.get_batch(0)};
        ml::simulation_tests::expect_equal(
            batch.tick, fixture.current_tick, "The event batch records its fixed tick");
        check_single_pair(batch.overlaps.entity_entity_overlaps, moved, stationary);
        check_single_static_overlap(batch.overlaps.entity_static_overlaps, moved, static_index);
    }
}

TEST(EntityAABBOverlaps, FrameEventResetRetainsStorageAndPassesAppend) {

    FOverlapFixture fixture;
    auto const static_index{fixture.add_static({{-10.f, -10.f, -10.f}}, {{10.f, 10.f, 10.f}})};
    auto const stationary{fixture.spawn({{0.f, 0.f, 0.f}})};
    auto const moved{fixture.spawn({{100.f, 0.f, 0.f}})};
    fixture.finish_spawning();

    std::array const handles{moved};
    std::array const locations{ml::simulation::Vector3f{{15.f, 0.f, 0.f}}};
    std::array const rotations{ml::simulation::Rotator3f{}};
    fixture.run_tick(handles, locations, rotations);

    auto& collision_system{fixture.query_manager.get_collision_system()};
    auto const first_events{collision_system.get_aabb_overlap_events()};
    check_single_pair(first_events.entity_entity_overlaps, moved, stationary);
    check_single_static_overlap(first_events.entity_static_overlaps, moved, static_index);
    auto const* const entity_storage{first_events.entity_entity_overlaps.first_entities.data()};
    auto const* const static_storage{first_events.entity_static_overlaps.entities.data()};

    collision_system.reset_frame_events();

    auto const reset_events{collision_system.get_aabb_overlap_events()};
    ml::simulation_tests::expect_equal(
        reset_events.entity_entity_overlaps.num(), 0, "Frame reset clears dynamic events");
    ml::simulation_tests::expect_equal(
        reset_events.entity_static_overlaps.num(), 0, "Frame reset clears static events");
    ml::simulation_tests::expect_equal(static_cast<std::int32_t>(reset_events.batches.size()),
                                       0,
                                       "Frame reset clears event batches");
    ml::simulation_tests::expect_true(reset_events.entity_entity_overlaps.first_entities.data() ==
                                          entity_storage,
                                      "Dynamic event storage is retained across reset");
    ml::simulation_tests::expect_true(reset_events.entity_static_overlaps.entities.data() ==
                                          static_storage,
                                      "Static event storage is retained across reset");

    collision_system.update(
        std::span<FRegistryEntityHandle const>{
            handles.data(), static_cast<std::size_t>(static_cast<std::int32_t>(handles.size()))},
        ++fixture.current_tick);
    auto const recaptured_events{collision_system.get_aabb_overlap_events()};
    check_single_pair(recaptured_events.entity_entity_overlaps, moved, stationary);
    check_single_static_overlap(recaptured_events.entity_static_overlaps, moved, static_index);
    ml::simulation_tests::expect_true(
        recaptured_events.entity_entity_overlaps.first_entities.data() == entity_storage,
        "Dynamic event storage is reused after recapture");
    ml::simulation_tests::expect_true(recaptured_events.entity_static_overlaps.entities.data() ==
                                          static_storage,
                                      "Static event storage is reused after recapture");

    collision_system.update(
        std::span<FRegistryEntityHandle const>{
            handles.data(), static_cast<std::size_t>(static_cast<std::int32_t>(handles.size()))},
        ++fixture.current_tick);
    auto const appended_events{collision_system.get_aabb_overlap_events()};
    ml::simulation_tests::expect_equal(appended_events.entity_entity_overlaps.num(),
                                       2,
                                       "Collision passes append dynamic events within a frame");
    ml::simulation_tests::expect_equal(appended_events.entity_static_overlaps.num(),
                                       2,
                                       "Collision passes append static events within a frame");
    ml::simulation_tests::expect_equal(static_cast<std::int32_t>(appended_events.batches.size()),
                                       2,
                                       "Collision passes retain separate batch metadata");
    if (appended_events.batches.size() == 2) {
        ml::simulation_tests::expect_true(appended_events.batches[0].tick <
                                              appended_events.batches[1].tick,
                                          "Later passes retain their own fixed tick");
    }

    collision_system.reset_frame_events();
    auto const next_frame_events{collision_system.get_aabb_overlap_events()};
    ml::simulation_tests::expect_equal(next_frame_events.entity_entity_overlaps.num(),
                                       0,
                                       "The next frame starts without dynamic events");
    ml::simulation_tests::expect_equal(next_frame_events.entity_static_overlaps.num(),
                                       0,
                                       "The next frame starts without static events");
    ml::simulation_tests::expect_equal(static_cast<std::int32_t>(next_frame_events.batches.size()),
                                       0,
                                       "The next frame starts without event batches");
}

TEST(EntityAABBOverlaps, InvalidDeadAndStaleDirtyHandlesAreIgnored) {

    FOverlapFixture fixture;
    auto const static_index{fixture.add_static({{-20.f, -20.f, -20.f}}, {{20.f, 20.f, 20.f}})};
    auto const live{fixture.spawn({{0.f, 0.f, 0.f}})};
    auto const removed{fixture.spawn({{100.f, 0.f, 0.f}})};
    fixture.finish_spawning();

    std::array const removed_handle{removed};
    std::array const removed_location{ml::simulation::Vector3f{{10.f, 0.f, 0.f}}};
    std::array const rotations{ml::simulation::Rotator3f{}};
    std::array const dead{std::uint8_t{0}};
    fixture.run_tick(removed_handle, removed_location, rotations, dead);
    ml::simulation_tests::expect_true(fixture.registry.is_valid_dead(removed),
                                      "Moved-and-dead entity remains an active dead handle");
    ml::simulation_tests::expect_equal(
        fixture.get_entity_overlaps().num(), 0, "Dead dirty handle produces no pair");
    ml::simulation_tests::expect_equal(
        fixture.get_static_overlaps().num(), 0, "Dead dirty handle produces no static overlap");

    fixture.end_tick();
    auto const replacement{fixture.spawn({{10.f, 0.f, 0.f}})};
    ml::simulation_tests::expect_true(fixture.registry.is_stale(removed),
                                      "Removed handle becomes stale after slot reuse");

    std::array const dirty_entities{
        FRegistryEntityHandle{}, FRegistryEntityHandle{999, 0}, removed, live};
    fixture.query_manager.get_collision_system().update(
        std::span<FRegistryEntityHandle const>{
            dirty_entities.data(),
            static_cast<std::size_t>(static_cast<std::int32_t>(dirty_entities.size()))},
        ++fixture.current_tick);

    check_single_pair(fixture.get_entity_overlaps(), live, replacement);
    check_single_static_overlap(fixture.get_static_overlaps(), live, static_index);
}
