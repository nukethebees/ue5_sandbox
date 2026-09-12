#include <SpaceGameSimulation/entities/TestEntityRegistry.h>
#include <SpaceGameSimulation/simulation/EntityWorldBounds.h>
#include <SpaceGameSimulation/simulation/SpatialQueryManager.h>

#include <CQTest.h>

namespace {
struct FOverlapFixture {
    explicit FOverlapFixture(FVector3f const capital_half_extents = {10.f, 10.f, 10.f},
                             FVector3f const capital_centre = FVector3f::ZeroVector,
                             FVector3f const turret_half_extents = {10.f, 10.f, 10.f})
        : query_manager{registry} {
        auto const type_count{ml::ioj::FEntityAABBs::num()};
        for (int32 type_index{}; type_index < type_count; ++type_index) {
            set_bounds(type_index, FVector3f::ZeroVector, FVector3f{10.f, 10.f, 10.f});
        }
        set_bounds(ml::ioj::FEntityAABBs::capital_ship_index, capital_centre, capital_half_extents);
        set_bounds(
            ml::ioj::FEntityAABBs::static_turret_index, FVector3f::ZeroVector, turret_half_extents);

        query_manager.initialise({40, 40, 40}, {50.f, 50.f, 50.f}, entity_bounds);
    }

    auto spawn(FVector3f const location,
               ETestEntityType const type = ETestEntityType::CapitalShip,
               FRotator3f const rotation = FRotator3f::ZeroRotator) -> FRegistryEntityHandle {
        FTestEntityRegistry::EntityData data;
        data.locations.add(location);
        data.velocities.add(FVector3f::ZeroVector);
        data.rotations.add(rotation.Pitch, rotation.Yaw, rotation.Roll);
        data.radii.Add(1.f);
        data.healths.Add(100);
        data.teams.Add(ETestTeam::Blue);
        data.entity_types.Add(type);
        data.alive.Add(uint8{1});
        return registry.add_entities(data.get_const_view()).registry_handles[0];
    }

    void finish_spawning() {
        registry.commit_updates();
        query_manager.update();
        registry.end_tick();
        query_manager.get_collision_system().reset_frame_events();
    }

    auto add_static(FVector3f const min_point, FVector3f const max_point) -> int32 {
        return query_manager.get_collision_system().get_uniform_grid().add_static_aabb(min_point,
                                                                                       max_point);
    }

    void run_tick(TConstArrayView<FRegistryEntityHandle> const handles,
                  TConstArrayView<FVector3f> const locations,
                  TConstArrayView<FRotator3f> const rotations,
                  TConstArrayView<uint8> const alive = {}) {
        check(handles.Num() == locations.Num());
        check(handles.Num() == rotations.Num());
        check(alive.IsEmpty() || handles.Num() == alive.Num());

        query_manager.get_collision_system().reset_frame_events();
        registry.begin_tick();

        FTestEntityRegistry::EntityData updates;
        EntityDeathInfo deaths;
        auto const& current{registry.get_entity_data()};
        auto const count{handles.Num()};
        for (int32 index{}; index < count; ++index) {
            auto const handle{handles[index]};
            auto const entity_alive{alive.IsEmpty() ? uint8{1} : alive[index]};
            updates.locations.add(locations[index]);
            updates.velocities.add(current.velocities[handle.index]);
            updates.rotations.add(
                rotations[index].Pitch, rotations[index].Yaw, rotations[index].Roll);
            updates.radii.Add(current.radii[handle.index]);
            updates.healths.Add(current.healths[handle.index]);
            updates.teams.Add(current.teams[handle.index]);
            updates.entity_types.Add(current.entity_types[handle.index]);
            updates.alive.Add(entity_alive);
            if (entity_alive == 0) {
                deaths.add(ETestDeathReason::Unknown, handle);
            }
        }

        registry.queue_entity_updates({handles, updates.get_const_view()}, deaths);
        registry.commit_updates();
        query_manager.update();
        tick_is_open = true;
    }

    void run_quiet_tick() {
        end_tick();
        query_manager.get_collision_system().reset_frame_events();
        registry.begin_tick();
        registry.commit_updates();
        query_manager.update();
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

    void set_bounds(int32 const type_index, FVector3f const centre, FVector3f const half_extents) {
        entity_bounds.centre_xs[type_index] = centre.X;
        entity_bounds.centre_ys[type_index] = centre.Y;
        entity_bounds.centre_zs[type_index] = centre.Z;
        entity_bounds.half_extent_xs[type_index] = half_extents.X;
        entity_bounds.half_extent_ys[type_index] = half_extents.Y;
        entity_bounds.half_extent_zs[type_index] = half_extents.Z;
    }

    FTestEntityRegistry registry;
    ml::FSpatialQueryManager query_manager;
    ml::ioj::FEntityAABBs entity_bounds;
    bool tick_is_open{};
};

void check_single_pair(FAutomationTestBase* const test,
                       ml::ioj::FEntityEntityOverlaps::ConstView const overlaps,
                       FRegistryEntityHandle const lhs,
                       FRegistryEntityHandle const rhs) {
    test->TestEqual(TEXT("Exactly one overlap pair is reported"), overlaps.num(), 1);
    if (overlaps.num() == 1) {
        auto const first{lhs < rhs ? lhs : rhs};
        auto const second{lhs < rhs ? rhs : lhs};
        test->TestTrue(TEXT("Overlap pair is canonical"),
                       overlaps.first_entities[0] == first &&
                           overlaps.second_entities[0] == second);
        test->TestTrue(TEXT("First overlap handle sorts before second"),
                       overlaps.first_entities[0] < overlaps.second_entities[0]);
    }
}

void check_single_static_overlap(FAutomationTestBase* const test,
                                 ml::ioj::FEntityStaticOverlaps::ConstView const overlaps,
                                 FRegistryEntityHandle const entity,
                                 int32 const static_geometry_index) {
    test->TestEqual(TEXT("Exactly one static overlap is reported"), overlaps.num(), 1);
    if (overlaps.num() == 1) {
        test->TestTrue(TEXT("Static overlap identity is correct"),
                       overlaps.entities[0] == entity &&
                           overlaps.static_geometry_indices[0] == static_geometry_index);
    }
}
}

TEST_CLASS(EntityAABBOverlaps, "Sandbox.UnitTests")
{
    TEST_METHOD(MovedEntityOverlapsStationaryEntity)
    {
        FOverlapFixture fixture;
        auto const stationary{fixture.spawn({0.f, 0.f, 0.f})};
        auto const moved{fixture.spawn({100.f, 0.f, 0.f})};
        fixture.finish_spawning();

        TArray const handles{moved};
        TArray const locations{FVector3f{15.f, 0.f, 0.f}};
        TArray const rotations{FRotator3f::ZeroRotator};
        fixture.run_tick(handles, locations, rotations);

        check_single_pair(TestRunner, fixture.get_entity_overlaps(), moved, stationary);
        TestRunner->TestEqual(TEXT("A dynamic-only overlap produces no static record"),
                              fixture.get_static_overlaps().num(),
                              0);
    }

    TEST_METHOD(TwoMovedEntitiesProduceOnePair)
    {
        FOverlapFixture fixture;
        auto const first{fixture.spawn({-100.f, 0.f, 0.f})};
        auto const second{fixture.spawn({100.f, 0.f, 0.f})};
        fixture.finish_spawning();

        TArray const handles{first, second};
        TArray const locations{FVector3f{-5.f, 0.f, 0.f}, FVector3f{5.f, 0.f, 0.f}};
        TArray const rotations{FRotator3f::ZeroRotator, FRotator3f::ZeroRotator};
        fixture.run_tick(handles, locations, rotations);

        check_single_pair(TestRunner, fixture.get_entity_overlaps(), first, second);
    }

    TEST_METHOD(SharedCellWithoutExactOverlapProducesNoPair)
    {
        FOverlapFixture fixture{{5.f, 5.f, 5.f}};
        auto const stationary{fixture.spawn({40.f, 10.f, 10.f})};
        auto const moved{fixture.spawn({20.f, 10.f, 10.f})};
        fixture.finish_spawning();

        TArray const handles{moved};
        TArray const locations{FVector3f{10.f, 10.f, 10.f}};
        TArray const rotations{FRotator3f::ZeroRotator};
        fixture.run_tick(handles, locations, rotations);

        TestRunner->TestEqual(TEXT("Broad-phase cell sharing is rejected by exact AABB testing"),
                              fixture.get_entity_overlaps().num(),
                              0);
        TestRunner->TestTrue(TEXT("Both entities remain in the same grid cell"),
                             fixture.query_manager.get_collision_system()
                                     .get_uniform_grid()
                                     .get_cell_entities({20, 20, 20})
                                     .Contains(stationary) &&
                                 fixture.query_manager.get_collision_system()
                                     .get_uniform_grid()
                                     .get_cell_entities({20, 20, 20})
                                     .Contains(moved));
    }

    TEST_METHOD(MultiCellOverlapProducesOnePair)
    {
        FOverlapFixture fixture{{120.f, 120.f, 120.f}};
        auto const stationary{fixture.spawn({0.f, 0.f, 0.f})};
        auto const moved{fixture.spawn({400.f, 0.f, 0.f})};
        fixture.finish_spawning();

        TArray const handles{moved};
        TArray const locations{FVector3f{10.f, 0.f, 0.f}};
        TArray const rotations{FRotator3f::ZeroRotator};
        fixture.run_tick(handles, locations, rotations);

        check_single_pair(TestRunner, fixture.get_entity_overlaps(), moved, stationary);
    }

    TEST_METHOD(SelfAndQuietTicksProduceNoPairs)
    {
        FOverlapFixture fixture;
        auto const entity{fixture.spawn({0.f, 0.f, 0.f})};
        fixture.finish_spawning();

        TArray const handles{entity};
        TArray const locations{FVector3f{1.f, 0.f, 0.f}};
        TArray const rotations{FRotator3f::ZeroRotator};
        fixture.run_tick(handles, locations, rotations);
        TestRunner->TestEqual(TEXT("Moved entity is never paired with itself"),
                              fixture.get_entity_overlaps().num(),
                              0);

        fixture.run_quiet_tick();
        TestRunner->TestEqual(TEXT("A quiet tick publishes an empty overlap result"),
                              fixture.get_entity_overlaps().num(),
                              0);
    }

    TEST_METHOD(RotatedConservativeWorldBoundsUseExistingBoundsRules)
    {
        FOverlapFixture fixture{{40.f, 5.f, 5.f}, {30.f, 0.f, 0.f}, {5.f, 5.f, 5.f}};
        auto const rotated{fixture.spawn({0.f, 0.f, 0.f})};
        auto const stationary{fixture.spawn({0.f, 60.f, 0.f}, ETestEntityType::Turret)};
        fixture.finish_spawning();

        auto const rotation{FRotator3f{0.f, 90.f, 0.f}};
        auto const expected_bounds{
            ml::ioj::make_entity_world_bounds(fixture.entity_bounds,
                                              ml::ioj::FEntityAABBs::capital_ship_index,
                                              FVector3f::ZeroVector,
                                              rotation)};
        TestRunner->TestTrue(TEXT("Rotated conservative bounds reach the stationary entity"),
                             expected_bounds.Min.Y <= 55.f && expected_bounds.Max.Y >= 65.f);

        TArray const handles{rotated};
        TArray const locations{FVector3f::ZeroVector};
        TArray const rotations{rotation};
        fixture.run_tick(handles, locations, rotations);

        check_single_pair(TestRunner, fixture.get_entity_overlaps(), rotated, stationary);
    }

    TEST_METHOD(MovingOutOfOverlapRemovesPairFromCurrentResult)
    {
        FOverlapFixture fixture;
        auto const stationary{fixture.spawn({0.f, 0.f, 0.f})};
        auto const moved{fixture.spawn({100.f, 0.f, 0.f})};
        fixture.finish_spawning();

        TArray const handles{moved};
        TArray const rotations{FRotator3f::ZeroRotator};
        TArray const overlapping_location{FVector3f{15.f, 0.f, 0.f}};
        fixture.run_tick(handles, overlapping_location, rotations);
        check_single_pair(TestRunner, fixture.get_entity_overlaps(), moved, stationary);

        fixture.end_tick();
        TArray const separated_location{FVector3f{100.f, 0.f, 0.f}};
        fixture.run_tick(handles, separated_location, rotations);
        TestRunner->TestEqual(TEXT("Separated entities are absent from the current result"),
                              fixture.get_entity_overlaps().num(),
                              0);
    }

    TEST_METHOD(MovedEntityOverlapsOneStaticAABB)
    {
        FOverlapFixture fixture;
        auto const static_index{fixture.add_static({-10.f, -10.f, -10.f}, {10.f, 10.f, 10.f})};
        auto const moved{fixture.spawn({100.f, 0.f, 0.f})};
        fixture.finish_spawning();

        TArray const handles{moved};
        TArray const locations{FVector3f{15.f, 0.f, 0.f}};
        TArray const rotations{FRotator3f::ZeroRotator};
        fixture.run_tick(handles, locations, rotations);

        check_single_static_overlap(TestRunner, fixture.get_static_overlaps(), moved, static_index);
        TestRunner->TestEqual(TEXT("A static-only overlap produces no dynamic pair"),
                              fixture.get_entity_overlaps().num(),
                              0);
    }

    TEST_METHOD(MovedEntityOverlapsMultipleStaticAABBs)
    {
        FOverlapFixture fixture;
        auto const first_static{fixture.add_static({-20.f, -20.f, -20.f}, {20.f, 20.f, 20.f})};
        auto const second_static{fixture.add_static({-5.f, -30.f, -5.f}, {5.f, 30.f, 5.f})};
        auto const moved{fixture.spawn({100.f, 0.f, 0.f})};
        fixture.finish_spawning();

        TArray const handles{moved};
        TArray const locations{FVector3f::ZeroVector};
        TArray const rotations{FRotator3f::ZeroRotator};
        fixture.run_tick(handles, locations, rotations);

        auto const overlaps{fixture.get_static_overlaps()};
        TestRunner->TestEqual(TEXT("Both static overlaps are reported"), overlaps.num(), 2);
        if (overlaps.num() == 2) {
            TestRunner->TestTrue(TEXT("Static overlaps are sorted by geometry index"),
                                 overlaps.entities[0] == moved &&
                                     overlaps.static_geometry_indices[0] == first_static &&
                                     overlaps.entities[1] == moved &&
                                     overlaps.static_geometry_indices[1] == second_static);
        }
    }

    TEST_METHOD(SharedCellWithoutExactStaticOverlapProducesNoRecord)
    {
        FOverlapFixture fixture{{5.f, 5.f, 5.f}};
        fixture.add_static({30.f, 5.f, 5.f}, {40.f, 15.f, 15.f});
        auto const moved{fixture.spawn({100.f, 10.f, 10.f})};
        fixture.finish_spawning();

        TArray const handles{moved};
        TArray const locations{FVector3f{10.f, 10.f, 10.f}};
        TArray const rotations{FRotator3f::ZeroRotator};
        fixture.run_tick(handles, locations, rotations);

        TestRunner->TestEqual(TEXT("Static broad-phase cell sharing requires exact overlap"),
                              fixture.get_static_overlaps().num(),
                              0);
    }

    TEST_METHOD(MultiCellStaticColliderProducesOneRecord)
    {
        FOverlapFixture fixture{{5.f, 5.f, 5.f}};
        auto const static_index{
            fixture.add_static({-120.f, -120.f, -120.f}, {120.f, 120.f, 120.f})};
        auto const moved{fixture.spawn({400.f, 0.f, 0.f})};
        fixture.finish_spawning();

        TArray const handles{moved};
        TArray const locations{FVector3f::ZeroVector};
        TArray const rotations{FRotator3f::ZeroRotator};
        fixture.run_tick(handles, locations, rotations);

        check_single_static_overlap(TestRunner, fixture.get_static_overlaps(), moved, static_index);
    }

    TEST_METHOD(MultiCellEntityProducesOneStaticRecord)
    {
        FOverlapFixture fixture{{120.f, 120.f, 120.f}};
        auto const static_index{fixture.add_static({-5.f, -5.f, -5.f}, {5.f, 5.f, 5.f})};
        auto const moved{fixture.spawn({400.f, 0.f, 0.f})};
        fixture.finish_spawning();

        TArray const handles{moved};
        TArray const locations{FVector3f{10.f, 0.f, 0.f}};
        TArray const rotations{FRotator3f::ZeroRotator};
        fixture.run_tick(handles, locations, rotations);

        check_single_static_overlap(TestRunner, fixture.get_static_overlaps(), moved, static_index);
    }

    TEST_METHOD(MultipleMovedEntitiesProduceDistinctStaticRecords)
    {
        FOverlapFixture fixture;
        auto const static_index{fixture.add_static({-20.f, -20.f, -20.f}, {20.f, 20.f, 20.f})};
        auto const first{fixture.spawn({-100.f, 0.f, 0.f})};
        auto const second{fixture.spawn({100.f, 0.f, 0.f})};
        fixture.finish_spawning();

        TArray const handles{first, second};
        TArray const locations{FVector3f{-10.f, 0.f, 0.f}, FVector3f{10.f, 0.f, 0.f}};
        TArray const rotations{FRotator3f::ZeroRotator, FRotator3f::ZeroRotator};
        fixture.run_tick(handles, locations, rotations);

        auto const overlaps{fixture.get_static_overlaps()};
        TestRunner->TestEqual(
            TEXT("Each moved entity has its own static overlap"), overlaps.num(), 2);
        if (overlaps.num() == 2) {
            TestRunner->TestTrue(TEXT("Static overlap records retain both identities"),
                                 overlaps.entities[0] == first &&
                                     overlaps.static_geometry_indices[0] == static_index &&
                                     overlaps.entities[1] == second &&
                                     overlaps.static_geometry_indices[1] == static_index);
        }
    }

    TEST_METHOD(MovedEntityProducesDynamicAndStaticOverlapsTogether)
    {
        FOverlapFixture fixture;
        auto const static_index{fixture.add_static({-10.f, -10.f, -10.f}, {10.f, 10.f, 10.f})};
        auto const stationary{fixture.spawn({0.f, 0.f, 0.f})};
        auto const moved{fixture.spawn({100.f, 0.f, 0.f})};
        fixture.finish_spawning();

        TArray const handles{moved};
        TArray const locations{FVector3f{15.f, 0.f, 0.f}};
        TArray const rotations{FRotator3f::ZeroRotator};
        fixture.run_tick(handles, locations, rotations);

        check_single_pair(TestRunner, fixture.get_entity_overlaps(), moved, stationary);
        check_single_static_overlap(TestRunner, fixture.get_static_overlaps(), moved, static_index);
    }

    TEST_METHOD(RotationOnlyMovementUsesConservativeBoundsForBothStreams)
    {
        FOverlapFixture fixture{{40.f, 5.f, 5.f}, {30.f, 0.f, 0.f}, {5.f, 5.f, 5.f}};
        auto const static_index{fixture.add_static({-5.f, 55.f, -5.f}, {5.f, 65.f, 5.f})};
        auto const rotated{fixture.spawn({0.f, 0.f, 0.f})};
        auto const stationary{fixture.spawn({0.f, 60.f, 0.f}, ETestEntityType::Turret)};
        fixture.finish_spawning();

        TArray const handles{rotated};
        TArray const locations{FVector3f::ZeroVector};
        TArray const rotations{FRotator3f{0.f, 90.f, 0.f}};
        fixture.run_tick(handles, locations, rotations);

        check_single_pair(TestRunner, fixture.get_entity_overlaps(), rotated, stationary);
        check_single_static_overlap(
            TestRunner, fixture.get_static_overlaps(), rotated, static_index);
    }

    TEST_METHOD(ManyMovedEntitiesProduceSortedUniqueResults)
    {
        FOverlapFixture fixture{{60.f, 60.f, 60.f}};
        auto const large_static{
            fixture.add_static({-450.f, -450.f, -100.f}, {450.f, 450.f, 100.f})};
        for (int32 index{}; index < 8; ++index) {
            auto const x{static_cast<float>((index % 4) * 160 - 240)};
            auto const y{static_cast<float>((index / 4) * 240 - 120)};
            fixture.add_static({x - 80.f, y - 80.f, -80.f}, {x + 80.f, y + 80.f, 80.f});
        }

        TArray<FRegistryEntityHandle> moved_entities;
        TArray<FVector3f> moved_locations;
        TArray<FRotator3f> moved_rotations;
        constexpr int32 entity_count{32};
        moved_entities.Reserve(entity_count);
        moved_locations.Reserve(entity_count);
        moved_rotations.Reserve(entity_count);
        for (int32 index{}; index < entity_count; ++index) {
            auto const x{static_cast<float>((index % 8) * 100 - 350)};
            auto const y{static_cast<float>((index / 8) * 100 - 150)};
            fixture.spawn({x, y, 0.f});
            moved_entities.Add(fixture.spawn({x, y, 500.f}));
            moved_locations.Add({x, y, 0.f});
            moved_rotations.Add(FRotator3f::ZeroRotator);
        }
        fixture.finish_spawning();
        fixture.run_tick(moved_entities, moved_locations, moved_rotations);

        auto const entity_overlaps{fixture.get_entity_overlaps()};
        TestRunner->TestTrue(TEXT("Large query produces dynamic overlaps"),
                             entity_overlaps.num() >= entity_count);
        for (int32 index{}; index < entity_overlaps.num(); ++index) {
            TestRunner->TestTrue(
                TEXT("Dynamic overlap handles remain valid"),
                fixture.registry.is_valid_alive(entity_overlaps.first_entities[index]) &&
                    fixture.registry.is_valid_alive(entity_overlaps.second_entities[index]));
            TestRunner->TestTrue(TEXT("Dynamic overlap is canonical"),
                                 entity_overlaps.first_entities[index] <
                                     entity_overlaps.second_entities[index]);
            if (index > 0) {
                auto const previous_first{entity_overlaps.first_entities[index - 1]};
                auto const previous_second{entity_overlaps.second_entities[index - 1]};
                auto const current_first{entity_overlaps.first_entities[index]};
                auto const current_second{entity_overlaps.second_entities[index]};
                TestRunner->TestTrue(
                    TEXT("Dynamic overlaps are sorted and unique"),
                    previous_first < current_first ||
                        (previous_first == current_first && previous_second < current_second));
            }
        }

        auto const static_overlaps{fixture.get_static_overlaps()};
        TestRunner->TestTrue(TEXT("Every moved entity overlaps the large static AABB"),
                             static_overlaps.num() >= entity_count);
        for (int32 index{}; index < static_overlaps.num(); ++index) {
            TestRunner->TestTrue(TEXT("Static overlap entity remains valid"),
                                 fixture.registry.is_valid_alive(static_overlaps.entities[index]));
            auto const static_geometry_count{fixture.query_manager.get_collision_system()
                                                 .get_uniform_grid()
                                                 .get_static_aabbs()
                                                 .num()};
            TestRunner->TestTrue(TEXT("Static overlap index remains valid"),
                                 static_overlaps.static_geometry_indices[index] >= 0 &&
                                     static_overlaps.static_geometry_indices[index] <
                                         static_geometry_count);
            if (index > 0) {
                auto const previous_entity{static_overlaps.entities[index - 1]};
                auto const previous_static{static_overlaps.static_geometry_indices[index - 1]};
                auto const current_entity{static_overlaps.entities[index]};
                auto const current_static{static_overlaps.static_geometry_indices[index]};
                TestRunner->TestTrue(
                    TEXT("Static overlaps are sorted and unique"),
                    previous_entity < current_entity ||
                        (previous_entity == current_entity && previous_static < current_static));
            }
        }
        for (auto const moved : moved_entities) {
            bool found_large_static{};
            for (int32 index{}; index < static_overlaps.num(); ++index) {
                found_large_static =
                    found_large_static ||
                    (static_overlaps.entities[index] == moved &&
                     static_overlaps.static_geometry_indices[index] == large_static);
            }
            TestRunner->TestTrue(TEXT("Moved entity retains its large-static identity"),
                                 found_large_static);
        }
    }

    TEST_METHOD(EventsMirrorAuthoritativeResults)
    {
        FOverlapFixture fixture;
        auto const static_index{fixture.add_static({-10.f, -10.f, -10.f}, {10.f, 10.f, 10.f})};
        auto const stationary{fixture.spawn({0.f, 0.f, 0.f})};
        auto const moved{fixture.spawn({100.f, 0.f, 0.f})};
        fixture.finish_spawning();

        TArray const handles{moved};
        TArray const locations{FVector3f{15.f, 0.f, 0.f}};
        TArray const rotations{FRotator3f::ZeroRotator};
        fixture.run_tick(handles, locations, rotations);

        auto const& collision_system{fixture.query_manager.get_collision_system()};
        check_single_pair(
            TestRunner, collision_system.get_entity_entity_overlaps(), moved, stationary);
        check_single_static_overlap(
            TestRunner, collision_system.get_entity_static_overlaps(), moved, static_index);
        check_single_pair(TestRunner, fixture.get_entity_overlaps(), moved, stationary);
        check_single_static_overlap(TestRunner, fixture.get_static_overlaps(), moved, static_index);
    }

    TEST_METHOD(FrameEventResetRetainsStorageAndPassesAppend)
    {
        FOverlapFixture fixture;
        auto const static_index{fixture.add_static({-10.f, -10.f, -10.f}, {10.f, 10.f, 10.f})};
        auto const stationary{fixture.spawn({0.f, 0.f, 0.f})};
        auto const moved{fixture.spawn({100.f, 0.f, 0.f})};
        fixture.finish_spawning();

        TArray const handles{moved};
        TArray const locations{FVector3f{15.f, 0.f, 0.f}};
        TArray const rotations{FRotator3f::ZeroRotator};
        fixture.run_tick(handles, locations, rotations);

        auto& collision_system{fixture.query_manager.get_collision_system()};
        auto const first_events{collision_system.get_aabb_overlap_events()};
        check_single_pair(TestRunner, first_events.entity_entity_overlaps, moved, stationary);
        check_single_static_overlap(
            TestRunner, first_events.entity_static_overlaps, moved, static_index);
        auto const* const entity_storage{
            first_events.entity_entity_overlaps.first_entities.GetData()};
        auto const* const static_storage{first_events.entity_static_overlaps.entities.GetData()};

        collision_system.reset_frame_events();

        auto const reset_events{collision_system.get_aabb_overlap_events()};
        TestRunner->TestEqual(TEXT("Frame reset clears dynamic events"),
                              reset_events.entity_entity_overlaps.num(),
                              0);
        TestRunner->TestEqual(
            TEXT("Frame reset clears static events"), reset_events.entity_static_overlaps.num(), 0);
        TestRunner->TestTrue(TEXT("Dynamic event storage is retained across reset"),
                             reset_events.entity_entity_overlaps.first_entities.GetData() ==
                                 entity_storage);
        TestRunner->TestTrue(TEXT("Static event storage is retained across reset"),
                             reset_events.entity_static_overlaps.entities.GetData() ==
                                 static_storage);

        collision_system.update(handles);
        auto const recaptured_events{collision_system.get_aabb_overlap_events()};
        check_single_pair(TestRunner, recaptured_events.entity_entity_overlaps, moved, stationary);
        check_single_static_overlap(
            TestRunner, recaptured_events.entity_static_overlaps, moved, static_index);
        TestRunner->TestTrue(TEXT("Dynamic event storage is reused after recapture"),
                             recaptured_events.entity_entity_overlaps.first_entities.GetData() ==
                                 entity_storage);
        TestRunner->TestTrue(TEXT("Static event storage is reused after recapture"),
                             recaptured_events.entity_static_overlaps.entities.GetData() ==
                                 static_storage);

        collision_system.update(handles);
        auto const appended_events{collision_system.get_aabb_overlap_events()};
        TestRunner->TestEqual(TEXT("Collision passes append dynamic events within a frame"),
                              appended_events.entity_entity_overlaps.num(),
                              2);
        TestRunner->TestEqual(TEXT("Collision passes append static events within a frame"),
                              appended_events.entity_static_overlaps.num(),
                              2);

        collision_system.reset_frame_events();
        auto const next_frame_events{collision_system.get_aabb_overlap_events()};
        TestRunner->TestEqual(TEXT("The next frame starts without dynamic events"),
                              next_frame_events.entity_entity_overlaps.num(),
                              0);
        TestRunner->TestEqual(TEXT("The next frame starts without static events"),
                              next_frame_events.entity_static_overlaps.num(),
                              0);
    }

    TEST_METHOD(InvalidDeadAndStaleDirtyHandlesAreIgnored)
    {
        FOverlapFixture fixture;
        auto const static_index{fixture.add_static({-20.f, -20.f, -20.f}, {20.f, 20.f, 20.f})};
        auto const live{fixture.spawn({0.f, 0.f, 0.f})};
        auto const removed{fixture.spawn({100.f, 0.f, 0.f})};
        fixture.finish_spawning();

        TArray const removed_handle{removed};
        TArray const removed_location{FVector3f{10.f, 0.f, 0.f}};
        TArray const rotations{FRotator3f::ZeroRotator};
        TArray const dead{uint8{0}};
        fixture.run_tick(removed_handle, removed_location, rotations, dead);
        TestRunner->TestTrue(TEXT("Moved-and-dead entity remains an active dead handle"),
                             fixture.registry.is_valid_dead(removed));
        TestRunner->TestEqual(
            TEXT("Dead dirty handle produces no pair"), fixture.get_entity_overlaps().num(), 0);
        TestRunner->TestEqual(TEXT("Dead dirty handle produces no static overlap"),
                              fixture.get_static_overlaps().num(),
                              0);

        fixture.end_tick();
        auto const replacement{fixture.spawn({10.f, 0.f, 0.f})};
        TestRunner->TestTrue(TEXT("Removed handle becomes stale after slot reuse"),
                             fixture.registry.is_stale(removed));

        TArray const dirty_entities{
            FRegistryEntityHandle{}, FRegistryEntityHandle{999, 0}, removed, live};
        fixture.query_manager.get_collision_system().update(dirty_entities);

        check_single_pair(TestRunner, fixture.get_entity_overlaps(), live, replacement);
        check_single_static_overlap(TestRunner, fixture.get_static_overlaps(), live, static_index);
    }
};
