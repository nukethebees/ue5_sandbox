#include <SpaceGameSimulation/entities/TestEntityRegistry.h>
#include <SpaceGameSimulation/simulation/EntityWorldBounds.h>
#include <SpaceGameSimulation/simulation/SpatialQueryManager.h>

#include <CQTest.h>

namespace {
using ml::ioj::FEntityOverlapPair;

auto canonical_pair(FRegistryEntityHandle const lhs, FRegistryEntityHandle const rhs)
    -> FEntityOverlapPair {
    return lhs < rhs ? FEntityOverlapPair{lhs, rhs} : FEntityOverlapPair{rhs, lhs};
}

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
    }

    void run_tick(TConstArrayView<FRegistryEntityHandle> const handles,
                  TConstArrayView<FVector3f> const locations,
                  TConstArrayView<FRotator3f> const rotations,
                  TConstArrayView<uint8> const alive = {}) {
        check(handles.Num() == locations.Num());
        check(handles.Num() == rotations.Num());
        check(alive.IsEmpty() || handles.Num() == alive.Num());

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

    auto get_pairs() const -> TConstArrayView<FEntityOverlapPair> {
        return query_manager.get_collision_system().get_overlap_pairs();
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
                       TConstArrayView<FEntityOverlapPair> const pairs,
                       FRegistryEntityHandle const lhs,
                       FRegistryEntityHandle const rhs) {
    test->TestEqual(TEXT("Exactly one overlap pair is reported"), pairs.Num(), 1);
    if (pairs.Num() == 1) {
        test->TestTrue(TEXT("Overlap pair is canonical"), pairs[0] == canonical_pair(lhs, rhs));
        test->TestTrue(TEXT("First overlap handle sorts before second"),
                       pairs[0].first < pairs[0].second);
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

        check_single_pair(TestRunner, fixture.get_pairs(), moved, stationary);
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

        check_single_pair(TestRunner, fixture.get_pairs(), first, second);
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
                              fixture.get_pairs().Num(),
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

        check_single_pair(TestRunner, fixture.get_pairs(), moved, stationary);
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
        TestRunner->TestEqual(
            TEXT("Moved entity is never paired with itself"), fixture.get_pairs().Num(), 0);

        fixture.run_quiet_tick();
        TestRunner->TestEqual(
            TEXT("A quiet tick publishes an empty overlap result"), fixture.get_pairs().Num(), 0);
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

        check_single_pair(TestRunner, fixture.get_pairs(), rotated, stationary);
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
        check_single_pair(TestRunner, fixture.get_pairs(), moved, stationary);

        fixture.end_tick();
        TArray const separated_location{FVector3f{100.f, 0.f, 0.f}};
        fixture.run_tick(handles, separated_location, rotations);
        TestRunner->TestEqual(TEXT("Separated entities are absent from the current result"),
                              fixture.get_pairs().Num(),
                              0);
    }

    TEST_METHOD(InvalidDeadAndStaleDirtyHandlesAreIgnored)
    {
        FOverlapFixture fixture;
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
            TEXT("Dead dirty handle produces no pair"), fixture.get_pairs().Num(), 0);

        fixture.end_tick();
        auto const replacement{fixture.spawn({10.f, 0.f, 0.f})};
        TestRunner->TestTrue(TEXT("Removed handle becomes stale after slot reuse"),
                             fixture.registry.is_stale(removed));

        TArray const dirty_entities{
            FRegistryEntityHandle{}, FRegistryEntityHandle{999, 0}, removed, live};
        fixture.query_manager.get_collision_system().update(dirty_entities);

        check_single_pair(TestRunner, fixture.get_pairs(), live, replacement);
    }
};
