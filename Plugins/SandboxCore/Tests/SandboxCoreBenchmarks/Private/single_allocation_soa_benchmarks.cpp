#include <SbxCoreExperiments/soa_types.h>

#include "single_allocation_soa_benchmark_counts.h"
#include "TestHarness.h"

#include <catch2/benchmark/catch_benchmark.hpp>

#include <algorithm>

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

TEST_CASE("SandboxCore.SingleAllocation.Timing.reserve_4096", "[benchmark]") {
    run_comparison(Operation::Reserve, 4096);
}

TEST_CASE("SandboxCore.SingleAllocation.Timing.reserve_65536", "[benchmark]") {
    run_comparison(Operation::Reserve, 65536);
}

TEST_CASE("SandboxCore.SingleAllocation.Timing.reserve_1048576", "[benchmark]") {
    run_comparison(Operation::Reserve, 1048576);
}

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
