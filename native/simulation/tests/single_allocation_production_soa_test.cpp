#include "support/simulation_test_support.h"

#include <ioj/sim/capital_entity_data.h>
#include <ioj/sim/capital_spawn_data.h>
#include <ioj/sim/fighter_entity_data.h>
#include <ioj/sim/fighter_spawn_queue.h>
#include <ioj/sim/frame_hit_details.h>
#include <ioj/sim/laser_frame_output.h>
#include <ioj/sim/laser_hit_details.h>
#include <ioj/sim/laser_soa.h>
#include <ioj/sim/levels/compiled_level_events.h>
#include <ioj/sim/spinner_entity_data.h>
#include <ioj/sim/turret_entity_data.h>
#include <sandbox/core/frame_memory_resource.h>

#include <type_traits>
#include <utility>

namespace ioj::sim::tests {
namespace {
template <typename Storage>
void expect_storage_lifecycle() {
    static_assert(sizeof(Storage) == 16);
    static_assert(std::is_nothrow_move_constructible_v<Storage>);
    static_assert(std::is_nothrow_move_assignable_v<Storage>);

    Storage source;
    source.reserve(2);
    source.add_defaulted(1);
    source.add_uninitialised(3);
    tests::expect_equal(source.num(), 4, "Storage grows through production add paths");
    source.get_const_view().columns().validate_array_sizes();

    Storage moved{std::move(source)};
    tests::expect_equal(moved.num(), 4, "Move construction preserves rows");
    Storage assigned;
    assigned = std::move(moved);
    tests::expect_equal(assigned.num(), 4, "Move assignment preserves rows");

    assigned.remove_at_swap(1, 1);
    tests::expect_equal(assigned.num(), 3, "Swap removal removes a row");
    assigned.reset();
    tests::expect_equal(assigned.num(), 0, "Reset clears rows");
}
} // namespace

TEST(NativeSimulation, ProductionSingleAllocationSoaLifecycle) {
    expect_storage_lifecycle<SingleAllocationFighterEntityData>();
    expect_storage_lifecycle<SingleAllocationTurretEntityData>();
    expect_storage_lifecycle<lasers::SingleAllocationLaserEntities>();
    expect_storage_lifecycle<lasers::SingleAllocationLaserSpawnRequests>();
    expect_storage_lifecycle<SingleAllocationCapitalEntityData>();
    expect_storage_lifecycle<SingleAllocationCapitalSpawnData>();
    expect_storage_lifecycle<SingleAllocationFighterSpawnQueue>();
    expect_storage_lifecycle<SingleAllocationSpinnerEntityData>();
    expect_storage_lifecycle<SingleAllocationLaserHitDetails>();
    expect_storage_lifecycle<SingleAllocationLevelCapitalSpawnEvents>();
    expect_storage_lifecycle<SingleAllocationLevelTurretSpawnEvents>();
    expect_storage_lifecycle<SingleAllocationLevelSpinnerSpawnEvents>();
}

TEST(NativeSimulation, ProductionSingleAllocationNestedViewsAndAppend) {
    SingleAllocationFighterEntityData fighters;
    fighters.add_defaulted(2);
    auto fighter_columns{fighters.get_view().columns()};
    fighter_columns.locations.set(0, HMM_V3(1.f, 2.f, 3.f));
    fighter_columns.locations.set(1, HMM_V3(4.f, 5.f, 6.f));
    fighter_columns.integral_biases[0] = 11;
    fighter_columns.integral_biases[1] = 22;

    SingleAllocationFighterEntityData appended;
    appended.append_from(fighters.get_const_view());
    auto const appended_columns{appended.get_const_view().columns()};
    tests::expect_equal(appended_columns.locations[1].Z, 6.f, "Nested vectors append");
    tests::expect_equal(appended_columns.integral_biases[1], 22u, "Scalar columns append");

    lasers::SingleAllocationLaserEntities lasers;
    lasers.add_defaulted(2);
    auto laser_columns{lasers.get_view().columns()};
    laser_columns.locations.set(0, HMM_V3(10.f, 20.f, 30.f));
    laser_columns.rotations.set(0, Rotator3f{1.f, 2.f, 3.f});
    laser_columns.damages[0] = 40;
    lasers.remove_at_swap(0, 1);
    tests::expect_equal(lasers.num(), 1, "Laser swap removal keeps columns synchronized");
    lasers.get_const_view().columns().validate_array_sizes();
}

TEST(NativeSimulation, SpinnerAndLaserHitSingleAllocationRowsStaySynchronized) {
    SingleAllocationSpinnerEntityData spinners;
    spinners.add_defaulted(65);
    auto spinner_columns{spinners.get_view().columns()};
    for (std::int32_t index{}; index < spinners.num(); ++index) {
        spinner_columns.locations.set(index, HMM_V3(static_cast<float>(index), 2.f, 3.f));
        spinner_columns.yaws[index] = static_cast<float>(index * 2);
        spinner_columns.next_fire_point_indices[index] = index;
    }

    SingleAllocationSpinnerEntityData appended_spinners;
    appended_spinners.append_from(spinners.get_const_view());
    appended_spinners.remove_at_swap(1, 1);
    auto const appended_spinner_columns{appended_spinners.get_const_view().columns()};
    tests::expect_equal(appended_spinner_columns.locations.xs[1],
                        64.f,
                        "Spinner nested locations follow swap removal");
    tests::expect_equal(
        appended_spinner_columns.yaws[1], 128.f, "Spinner scalar columns follow swap removal");
    tests::expect_equal(appended_spinner_columns.next_fire_point_indices[1],
                        64,
                        "Spinner fire points follow swap removal");

    SingleAllocationLaserHitDetails hits;
    hits.add_defaulted(65);
    auto hit_columns{hits.get_view().columns()};
    for (std::int32_t index{}; index < hits.num(); ++index) {
        hit_columns.locations.set(index, HMM_V3(static_cast<float>(index), 4.f, 5.f));
        hit_columns.emission_directions.set(index, HMM_V3(0.f, 1.f, 0.f));
        hit_columns.sources[index] = {Team::Blue, EntityType::Turret};
    }

    SingleAllocationLaserHitDetails appended_hits;
    appended_hits.append_from(hits.get_const_view());
    auto const appended_hit_columns{appended_hits.get_const_view().columns()};
    tests::expect_equal(
        appended_hit_columns.locations.xs[64], 64.f, "Laser hit nested locations append");
    tests::expect_true(appended_hit_columns.sources[64] ==
                           LaserSource{Team::Blue, EntityType::Turret},
                       "Laser hit sources append");
}

TEST(NativeSimulation, LaserFrameOutputAccumulatesBatchesAndReusesStorage) {
    ml::FrameMemoryResource frame_memory{1024 * 1024};
    lasers::FrameHitDetails first_batch{&frame_memory};
    for (std::int32_t index{}; index < 65; ++index) {
        first_batch.add(HMM_V3(static_cast<float>(index), 1.f, 2.f),
                        HMM_V3(0.f, 1.f, 0.f),
                        {Team::Green, EntityType::Fighter});
    }
    lasers::FrameHitDetails second_batch{&frame_memory};
    second_batch.add(
        HMM_V3(100.f, 3.f, 4.f), HMM_V3(1.f, 0.f, 0.f), {Team::Red, EntityType::CapitalShip});
    second_batch.add(
        HMM_V3(101.f, 3.f, 4.f), HMM_V3(1.f, 0.f, 0.f), {Team::Red, EntityType::CapitalShip});

    lasers::FrameOutput output;
    output.append_hits(first_batch.get_const_view(), 7);
    output.append_hits(second_batch.get_const_view(), 9);

    auto const accumulated{output.hits.get_const_view().columns()};
    tests::expect_equal(accumulated.num(), 67, "All laser-hit batches accumulate");
    tests::expect_equal(accumulated.locations.xs[64], 64.f, "Growth preserves earlier hits");
    tests::expect_equal(accumulated.locations.xs[66], 101.f, "Later hits append in order");
    tests::expect_equal(output.hit_ticks[64], SimTick{7}, "First batch tick remains aligned");
    tests::expect_equal(output.hit_ticks[65], SimTick{9}, "Second batch tick remains aligned");
    tests::expect_equal(output.hit_ordinals[64], 64, "First batch ordinal remains aligned");
    tests::expect_equal(output.hit_ordinals[65], 0, "Each batch restarts hit ordinals");

    output.reset();
    tests::expect_equal(output.hits.num(), 0, "Reset clears accumulated hit rows");
    tests::expect_true(output.hit_ticks.empty(), "Reset clears hit ticks");
    tests::expect_true(output.hit_ordinals.empty(), "Reset clears hit ordinals");

    output.append_hits(second_batch.get_const_view(), 11);
    auto const reused{output.hits.get_const_view().columns()};
    tests::expect_equal(reused.num(), 2, "Reset storage can be reused");
    tests::expect_equal(reused.locations.xs[0], 100.f, "Reused storage receives new contents");
    tests::expect_equal(output.hit_ticks[0], SimTick{11}, "Reused tick side array stays aligned");
}

TEST(NativeSimulation, LevelEventSingleAllocationStoragePreservesOrderAcrossMoves) {
    CompiledLevelEvents events;
    constexpr std::int32_t row_count{65};
    for (std::int32_t index{}; index < row_count; ++index) {
        events.initial_spawns.capital_spawns.add_defaulted(1);
        auto const capitals{events.initial_spawns.capital_spawns.get_view().columns()};
        capitals.entity_indices[index] = index;
        capitals.locations.set(index, HMM_V3(static_cast<float>(index), 1.f, 2.f));
        capitals.teams[index] = Team::Green;
        capitals.healths[index] = 1000 + index;

        events.initial_spawns.turret_spawns.add_defaulted(1);
        auto const turrets{events.initial_spawns.turret_spawns.get_view().columns()};
        turrets.entity_indices[index] = index + 100;
        turrets.locations.set(index, HMM_V3(static_cast<float>(index), 3.f, 4.f));
        turrets.laser_damages[index] = index * 2;

        events.initial_spawns.spinner_spawns.add_defaulted(1);
        auto const spinners{events.initial_spawns.spinner_spawns.get_view().columns()};
        spinners.entity_indices[index] = index + 200;
        spinners.locations.set(index, HMM_V3(static_cast<float>(index), 5.f, 6.f));
        spinners.yaws[index] = static_cast<float>(index * 3);
        spinners.initial_fire_point_indices[index] = index;
    }
    events.schedule.capital_spawns.append_from(
        events.initial_spawns.capital_spawns.get_const_view());
    events.schedule.turret_spawns.append_from(events.initial_spawns.turret_spawns.get_const_view());

    CompiledLevelEvents moved{std::move(events)};
    auto const capitals{moved.initial_spawns.capital_spawns.get_const_view().columns()};
    auto const turrets{moved.schedule.turret_spawns.get_const_view().columns()};
    auto const spinners{moved.initial_spawns.spinner_spawns.get_const_view().columns()};
    tests::expect_equal(capitals.entity_indices[64], 64, "Capital event order survives growth");
    tests::expect_equal(capitals.locations.xs[64], 64.f, "Capital nested values survive moves");
    tests::expect_equal(turrets.entity_indices[64], 164, "Scheduled turret order is unchanged");
    tests::expect_equal(turrets.laser_damages[64], 128, "Scheduled turret payloads append");
    tests::expect_equal(spinners.entity_indices[64], 264, "Spinner event order survives growth");
    tests::expect_equal(spinners.yaws[64], 192.f, "Spinner payloads survive moves");
}
} // namespace ioj::sim::tests
