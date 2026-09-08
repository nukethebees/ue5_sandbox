#include <SbxCoreExperiments/soa_types.h>

#include <HAL/UnrealMemory.h>

#include <SbxCoreExperiments/soa_reference_allocators.h>
#include <SbxCoreExperiments/soa_test_support.h>
#include "single_allocation_soa_benchmark_counts.h"
#include "TestHarness.h"

#include <catch2/benchmark/catch_benchmark.hpp>

#include <algorithm>
#include <vector>

#include <string>

namespace ml::single_allocation_benchmarks {
using namespace single_allocation_experiment;

enum class Operation {
    NaturalAppend,
    ReservedAppend,
    DefaultedAppend,
    Reserve,
    Growth,
    SetNum,
    SetNumShrink,
    ResetReuse,
    RemoveSwap,
    Iterate,
    IterateWide,
    Views,
};

auto operation_name(Operation const operation, int32 const batch) -> std::string {
    switch (operation) {
        case Operation::NaturalAppend:
            return "natural_append_" + std::to_string(batch);
        case Operation::ReservedAppend:
            return "reserved_append_" + std::to_string(batch);
        case Operation::DefaultedAppend:
            return "defaulted_append_" + std::to_string(batch);
        case Operation::Reserve:
            return "reserve";
        case Operation::Growth:
            return "populated_growth";
        case Operation::SetNum:
            return "set_num_grow";
        case Operation::SetNumShrink:
            return "set_num_shrink_reuse";
        case Operation::ResetReuse:
            return "reset_reuse";
        case Operation::RemoveSwap:
            return "remove_swap";
        case Operation::Iterate:
            return "iterate";
        case Operation::IterateWide:
            return "iterate_wide";
        case Operation::Views:
            return "construct_views";
    }
    FAIL("Unknown benchmark operation");
    return {};
}
template <typename Owner>
FORCENOINLINE void append(Owner& owner, int32 const count, bool const defaulted) {
    if (defaulted) {
        owner.add_defaulted(count);
    } else {
        owner.add_uninitialised(count);
    }
}

template <typename Owner>
FORCENOINLINE void reset_reuse(Owner& owner, int32 const count) {
    owner.reset();
    owner.add_uninitialised(count);
}

template <typename Owner>
FORCENOINLINE void shrink_reuse(Owner& owner, int32 const count) {
    owner.set_num(count - 1, EAllowShrinking::No);
    owner.add_uninitialised(1);
}

FORCENOINLINE auto consume_view(EntityDataView const view) -> int32 {
    return view.healths.Num() + view.locations.xs.Num();
}

FORCENOINLINE void iterate(EntityDataView const view, bool const wide) {
    auto const count{view.num()};
    for (int32 pass{}; pass < 4; ++pass) {
        for (int32 index{}; index < count; ++index) {
            view.locations.xs[index] += view.velocities.xs[index] * 0.125f;
            view.locations.ys[index] += view.velocities.ys[index] * 0.125f;
            view.locations.zs[index] += view.velocities.zs[index] * 0.125f;
            if (wide) {
                view.desired_move_locations.xs[index] += view.movement_directions.xs[index];
                view.desired_move_locations.ys[index] += view.movement_directions.ys[index];
                view.desired_move_locations.zs[index] += view.movement_directions.zs[index];
                view.target_locations.xs[index] += view.target_velocities.xs[index] * 0.125f;
                view.target_locations.ys[index] += view.target_velocities.ys[index] * 0.125f;
                view.target_locations.zs[index] += view.target_velocities.zs[index] * 0.125f;
                view.target_distances[index] += view.speeds[index] * 0.125f;
                view.intercept_times[index] += view.float_biases[index];
            }
        }
    }
}

void initialise(EntityDataView const view) {
    auto const count{view.num()};
    for (int32 index{}; index < count; ++index) {
        view.healths[index] = index;
        view.velocities.xs[index] = 1.f;
        view.velocities.ys[index] = 2.f;
        view.velocities.zs[index] = 3.f;
        view.movement_directions.xs[index] = 0.25f;
        view.movement_directions.ys[index] = 0.5f;
        view.movement_directions.zs[index] = 0.75f;
        view.target_velocities.xs[index] = 4.f;
        view.target_velocities.ys[index] = 5.f;
        view.target_velocities.zs[index] = 6.f;
        view.speeds[index] = 7.f;
        view.float_biases[index] = 0.125f;
    }
}

template <typename Owner>
void prepare(Owner& owner, Operation const operation, int32 const count) {
    switch (operation) {
        case Operation::NaturalAppend:
        case Operation::Reserve:
            break;
        case Operation::ReservedAppend:
        case Operation::DefaultedAppend:
        case Operation::SetNum:
            owner.reserve(count);
            break;
        default:
            owner.reserve(count);
            owner.add_defaulted(count);
            initialise(owner.get_view());
            break;
    }
}

template <typename Owner>
FORCENOINLINE auto execute(Owner& owner, Operation const operation, int32 const count, int32 const batch) -> int64 {
    switch (operation) {
        case Operation::NaturalAppend:
        case Operation::ReservedAppend:
        case Operation::DefaultedAppend:
            for (int32 index{}; index < count; index += batch) {
                append(owner, std::min(batch, count - index), operation == Operation::DefaultedAppend);
            }
            break;
        case Operation::Reserve:
            owner.reserve(count);
            break;
        case Operation::Growth:
            // A large request forces both owners to move every populated leaf, regardless of allocator slack.
            owner.reserve(count * 2 + 1);
            break;
        case Operation::SetNum:
            owner.set_num(count, EAllowShrinking::No);
            break;
        case Operation::SetNumShrink:
            for (int32 iteration{}; iteration < 4096; ++iteration) {
                shrink_reuse(owner, count);
            }
            break;
        case Operation::ResetReuse:
            for (int32 iteration{}; iteration < 4096; ++iteration) {
                reset_reuse(owner, count);
            }
            break;
        case Operation::RemoveSwap:
            for (int32 iteration{}; iteration < count / 4; ++iteration) {
                owner.remove_at_swap(iteration, 1, EAllowShrinking::No);
            }
            break;
        case Operation::Views: {
            int64 total{};
            for (int32 iteration{}; iteration < 4096; ++iteration) {
                total += consume_view(owner.get_view());
            }
            return total;
        }
        case Operation::Iterate:
        case Operation::IterateWide:
            FAIL("Iteration must use the shared kernel with views constructed outside timing");
            break;
    }
    return owner.num();
}

template <typename Owner>
void run_once(Operation const operation, int32 const count, int32 const batch) {
    Owner owner;
    prepare(owner, operation, count);
    bool const iteration{operation == Operation::Iterate || operation == Operation::IterateWide};
    auto view{owner.get_view()};
    int64 observable{};
    if (iteration) {
        iterate(view, operation == Operation::IterateWide);
    } else {
        observable = execute(owner, operation, count, batch);
    }
    REQUIRE(observable >= 0);
    if (iteration) {
        REQUIRE(owner.get_view().locations.zs[count - 1] == 1.5f);
        if (operation == Operation::IterateWide) {
            REQUIRE(owner.get_view().target_locations.zs[count - 1] == 3.f);
        }
    } else if (operation == Operation::RemoveSwap) {
        REQUIRE(owner.num() == count - count / 4);
    } else if (operation == Operation::Reserve) {
        REQUIRE(owner.num() == 0);
    } else {
        REQUIRE(owner.num() == count);
    }
    if (operation == Operation::Growth) {
        REQUIRE(owner.get_view().healths[count - 1] == count - 1);
    }
}

TEST_CASE("SandboxCore.SingleAllocation.BenchmarkCorrectness") {
    auto check_allocator = []<typename Allocator> {
        TArray<uint32, Allocator> array;
        array.Reserve(64);
        REQUIRE(array.Max() == 64);
        REQUIRE(reinterpret_cast<UPTRINT>(array.GetData()) % 64 == 0);
        array.Add(42);
        array.Reserve(129);
        REQUIRE(array.Max() == 129);
        REQUIRE(array.Num() == 1);
        REQUIRE(array[0] == 42);
        REQUIRE(reinterpret_cast<UPTRINT>(array.GetData()) % 64 == 0);
        array.Empty();
        REQUIRE(array.GetData() == nullptr);
    };
    check_allocator.operator()<MallocAllocator>();
    check_allocator.operator()<ReallocAllocator>();
    check_allocator.operator()<MimallocArrayAllocator>();
    auto check_soa = []<typename Owner> {
        Owner owner;
        owner.reserve(129);
        int32 leaves{};
        each_leaf(owner, [&](auto const& column) {
            ++leaves;
            REQUIRE(column.Max() == 129);
            REQUIRE(reinterpret_cast<UPTRINT>(column.GetData()) % 64 == 0);
        });
        REQUIRE(leaves == 53);
        owner.add_defaulted(129);
        owner.healths[128] = 42;
        owner.locations.xs[128] = 3.f;
        owner.reserve(4096);
        REQUIRE(owner.num() == 129);
        REQUIRE(owner.healths[128] == 42);
        REQUIRE(owner.locations.xs[128] == 3.f);
    };
    check_soa.operator()<MallocEntityData>();
    check_soa.operator()<ReallocEntityData>();
    check_soa.operator()<MimallocEntityData>();
    {
        MimallocAlignmentData owner;
        owner.add_defaulted(129);
        owner.nested.xs[128] = 42.f;
        owner.reserve(4097);
        each_leaf(owner, [](auto const& column) {
            using Element = std::remove_cvref_t<decltype(column[0])>;
            REQUIRE(reinterpret_cast<UPTRINT>(column.GetData()) % std::max(SIZE_T{64}, alignof(Element)) == 0);
            REQUIRE(MimallocStorageAllocator::owns(column.GetData()));
        });
        auto moved{std::move(owner)};
        owner.add_defaulted(1);
        owner = std::move(moved);
        REQUIRE(owner.nested.xs[128] == 42.f);
        owner.reset();
        REQUIRE(owner.num() == 0);
    }
    {
        MimallocAlignmentDataSingle owner;
        REQUIRE(owner.num() == 0);
        for (int32 const count : {1, 63, 64, 65, 127, 128, 129, 4097}) {
            owner.set_num(count);
            REQUIRE(owner.capacity() % 64 == 0);
            each_leaf(owner.get_view(), [](auto column) {
                using Element = std::remove_reference_t<decltype(column[0])>;
                REQUIRE(reinterpret_cast<UPTRINT>(column.GetData()) % alignof(Element) == 0);
                REQUIRE(MimallocStorageAllocator::owns(column.GetData()));
            });
        }
        owner.get_view().nested.xs[0] = 42.f;
        auto moved{std::move(owner)};
        REQUIRE(owner.num() == 0);
        owner.set_num(64);
        owner = std::move(moved);
        REQUIRE(moved.num() == 0);
        REQUIRE(owner.get_const_view().nested.xs[0] == 42.f);
        owner.reset();
        REQUIRE(owner.num() == 0);
    }
    for (auto const operation : {Operation::NaturalAppend,
                                 Operation::ReservedAppend,
                                 Operation::DefaultedAppend,
                                 Operation::Reserve,
                                 Operation::Growth,
                                 Operation::SetNum,
                                 Operation::SetNumShrink,
                                 Operation::ResetReuse,
                                 Operation::RemoveSwap,
                                 Operation::Iterate,
                                 Operation::IterateWide,
                                 Operation::Views}) {
        auto const append_operation{operation == Operation::NaturalAppend || operation == Operation::ReservedAppend ||
                                    operation == Operation::DefaultedAppend};
        for (int32 const batch : {1, 64}) {
            if (batch == 64 && !append_operation) {
                continue;
            }
            CAPTURE(operation_name(operation, batch));
            run_once<EntityData>(operation, 129, batch);
            run_once<SingleAllocationEntityData>(operation, 129, batch);
            run_once<MimallocEntityDataSingle>(operation, 129, batch);
        }
    }
}
template <typename Owner>
void benchmark_owner(Operation const operation, int32 const count, int32 const batch, char const* const label) {
    auto const name{std::string{"SOA,"} + std::to_string(count) + "," + operation_name(operation, batch) + "," + label};
    BENCHMARK_ADVANCED(std::string{name})(Catch::Benchmark::Chronometer meter) {
        // Fresh allocation lifecycles keep memory bounded independently of Catch2's calibrated run count.
        if (operation == Operation::NaturalAppend || operation == Operation::Reserve || operation == Operation::Growth) {
            meter.measure([&] {
                Owner owner;
                prepare(owner, operation, count);
                return execute(owner, operation, count, batch);
            });
            return;
        }

        Owner owner;
        prepare(owner, operation, count);
        auto const view{owner.get_view()};
        meter.measure([&] {
            switch (operation) {
                case Operation::Iterate:
                case Operation::IterateWide:
                    iterate(view, operation == Operation::IterateWide);
                    return int64{view.num()};
                case Operation::ReservedAppend:
                case Operation::DefaultedAppend:
                case Operation::SetNum:
                    owner.reset();
                    break;
                case Operation::RemoveSwap:
                    owner.set_num(count, EAllowShrinking::No);
                    break;
                default:
                    break;
            }
            return execute(owner, operation, count, batch);
        });
    };
}

template <typename Owner>
void benchmark_reserve(int32 const count, std::size_t const owner_count, std::string const& name) {
    BENCHMARK(std::string{name}) {
        std::vector<Owner> owners(owner_count);
        for (auto& owner : owners) {
            owner.reserve(count);
        }
    };
}

template <bool UseRealloc>
void benchmark_raw_reserve(int32 const count, std::size_t const owner_count, std::string const& name) {
    auto const capacity{rounded_capacity(count, SingleAllocationEntityData::block_bytes)};
    auto const byte_count{allocation_bytes(capacity, SingleAllocationEntityData::block_bytes)};
    auto constexpr alignment{static_cast<uint32>(SingleAllocationEntityData::allocation_alignment)};
    BENCHMARK(std::string{name}) {
        std::vector<void*> allocations(owner_count);
        for (auto& allocation : allocations) {
            if constexpr (UseRealloc) {
                allocation = FMemory::Realloc(nullptr, byte_count, alignment);
            } else {
                allocation = FMemory::Malloc(byte_count, alignment);
            }
        }
        for (auto* const allocation : allocations) {
            FMemory::Free(allocation);
        }
    };
}
void run_comparison(Operation const operation, int32 const count, int32 const batch = 1) {
    benchmark_owner<EntityData>(operation, count, batch, "TArray");
    benchmark_owner<SingleAllocationEntityData>(operation, count, batch, "Single");
}
void run_comparisons(Operation const operation, int32 const batch = 1) {
    for (int32 const count : counts()) {
        run_comparison(operation, count, batch);
    }
}
TEST_CASE("SandboxCore.SingleAllocation.Timing.natural_append_1", "[benchmark]") {
    run_comparisons(Operation::NaturalAppend, 1);
}

TEST_CASE("SandboxCore.SingleAllocation.Timing.natural_append_64", "[benchmark]") {
    run_comparisons(Operation::NaturalAppend, 64);
}

TEST_CASE("SandboxCore.SingleAllocation.Timing.reserved_append_1", "[benchmark]") {
    run_comparisons(Operation::ReservedAppend, 1);
}

TEST_CASE("SandboxCore.SingleAllocation.Timing.reserved_append_64", "[benchmark]") {
    run_comparisons(Operation::ReservedAppend, 64);
}

TEST_CASE("SandboxCore.SingleAllocation.Timing.defaulted_append_1", "[benchmark]") {
    run_comparisons(Operation::DefaultedAppend, 1);
}

TEST_CASE("SandboxCore.SingleAllocation.Timing.defaulted_append_64", "[benchmark]") {
    run_comparisons(Operation::DefaultedAppend, 64);
}

#define SANDBOX_RESERVE_CASE(rows, owners, label, function)                                                        \
    TEST_CASE("SandboxCore.SingleAllocation.Timing.reserve_" #rows "_owners_" #owners "_" #label, "[benchmark]") { \
        function(rows, owners, "SOA," #rows ",reserve_" #owners "," #label);                                       \
    }

#define SANDBOX_RESERVE_CASES(rows, owners)                                                         \
    SANDBOX_RESERVE_CASE(rows, owners, TArray, benchmark_reserve<EntityData>)                       \
    SANDBOX_RESERVE_CASE(rows, owners, Single, benchmark_reserve<SingleAllocationEntityData>)       \
    SANDBOX_RESERVE_CASE(rows, owners, SingleMimalloc, benchmark_reserve<MimallocEntityDataSingle>) \
    SANDBOX_RESERVE_CASE(rows, owners, SoAMimalloc, benchmark_reserve<MimallocEntityData>)          \
    SANDBOX_RESERVE_CASE(rows, owners, RawMalloc, benchmark_raw_reserve<false>)                     \
    SANDBOX_RESERVE_CASE(rows, owners, RawRealloc, benchmark_raw_reserve<true>)                     \
    SANDBOX_RESERVE_CASE(rows, owners, SoAMalloc, benchmark_reserve<MallocEntityData>)              \
    SANDBOX_RESERVE_CASE(rows, owners, SoARealloc, benchmark_reserve<ReallocEntityData>)

SANDBOX_RESERVE_CASES(4096, 1)
SANDBOX_RESERVE_CASES(4096, 2)
SANDBOX_RESERVE_CASES(4096, 4)
SANDBOX_RESERVE_CASES(4096, 8)
SANDBOX_RESERVE_CASES(4096, 16)
SANDBOX_RESERVE_CASES(4096, 32)
SANDBOX_RESERVE_CASES(4096, 64)
SANDBOX_RESERVE_CASES(4096, 128)
SANDBOX_RESERVE_CASES(4096, 200)
SANDBOX_RESERVE_CASES(4096, 256)
SANDBOX_RESERVE_CASES(4096, 512)
SANDBOX_RESERVE_CASES(65536, 1)
SANDBOX_RESERVE_CASES(65536, 2)
SANDBOX_RESERVE_CASES(65536, 4)
SANDBOX_RESERVE_CASES(65536, 8)
SANDBOX_RESERVE_CASES(65536, 16)
SANDBOX_RESERVE_CASES(65536, 32)
SANDBOX_RESERVE_CASES(65536, 64)
SANDBOX_RESERVE_CASES(65536, 128)
SANDBOX_RESERVE_CASES(65536, 200)
SANDBOX_RESERVE_CASES(65536, 256)
SANDBOX_RESERVE_CASES(65536, 512)
SANDBOX_RESERVE_CASES(1048576, 1)
SANDBOX_RESERVE_CASES(1048576, 2)
SANDBOX_RESERVE_CASES(1048576, 4)
SANDBOX_RESERVE_CASES(1048576, 8)
SANDBOX_RESERVE_CASES(1048576, 16)
SANDBOX_RESERVE_CASES(1048576, 32)
SANDBOX_RESERVE_CASES(1048576, 64)
SANDBOX_RESERVE_CASES(1048576, 128)
SANDBOX_RESERVE_CASES(1048576, 200)
SANDBOX_RESERVE_CASES(1048576, 256)
SANDBOX_RESERVE_CASES(1048576, 512)

#undef SANDBOX_RESERVE_CASES
#undef SANDBOX_RESERVE_CASE

TEST_CASE("SandboxCore.SingleAllocation.Timing.populated_growth", "[benchmark]") {
    run_comparisons(Operation::Growth);
}

TEST_CASE("SandboxCore.SingleAllocation.Timing.set_num_grow", "[benchmark]") {
    run_comparisons(Operation::SetNum);
}

TEST_CASE("SandboxCore.SingleAllocation.Timing.set_num_shrink_reuse", "[benchmark]") {
    run_comparisons(Operation::SetNumShrink);
}

TEST_CASE("SandboxCore.SingleAllocation.Timing.reset_reuse", "[benchmark]") {
    run_comparisons(Operation::ResetReuse);
}

TEST_CASE("SandboxCore.SingleAllocation.Timing.remove_swap", "[benchmark]") {
    run_comparisons(Operation::RemoveSwap);
}

TEST_CASE("SandboxCore.SingleAllocation.Timing.iterate", "[benchmark]") {
    run_comparisons(Operation::Iterate);
}

TEST_CASE("SandboxCore.SingleAllocation.Timing.iterate_wide", "[benchmark]") {
    run_comparisons(Operation::IterateWide);
}

TEST_CASE("SandboxCore.SingleAllocation.Timing.construct_views", "[benchmark]") {
    run_comparisons(Operation::Views);
}
}
