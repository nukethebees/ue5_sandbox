#include "support/simulation_test_support.h"

#include <ioj/sim/capital_entity_data.h>
#include <ioj/sim/capital_spawn_data.h>
#include <ioj/sim/fighter_entity_data.h>
#include <ioj/sim/fighter_spawn_queue.h>
#include <ioj/sim/laser_soa.h>
#include <ioj/sim/registry_entity_data.h>
#include <ioj/sim/turret_entity_data.h>

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
    expect_storage_lifecycle<SingleAllocationRegistryEntityData>();
    expect_storage_lifecycle<SingleAllocationCapitalEntityData>();
    expect_storage_lifecycle<SingleAllocationCapitalSpawnData>();
    expect_storage_lifecycle<SingleAllocationFighterSpawnQueue>();
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
} // namespace ioj::sim::tests
