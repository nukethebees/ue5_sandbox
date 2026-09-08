#include <SbxCoreExperiments/soa_test_support.h>
#include <SbxCoreExperiments/soa_types.h>

#include "benchmark_cli_args.h"
#include "TestHarness.h"

#include <HAL/PlatformTime.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <type_traits>

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

struct Scenario {
    char const* name;
    Operation operation;
    int32 batch;
};

inline constexpr std::array scenarios{
    Scenario{"natural_append_1", Operation::NaturalAppend, 1},
    Scenario{"natural_append_64", Operation::NaturalAppend, 64},
    Scenario{"reserved_append_1", Operation::ReservedAppend, 1},
    Scenario{"reserved_append_64", Operation::ReservedAppend, 64},
    Scenario{"defaulted_append_1", Operation::DefaultedAppend, 1},
    Scenario{"defaulted_append_64", Operation::DefaultedAppend, 64},
    Scenario{"reserve", Operation::Reserve, 1},
    Scenario{"populated_growth", Operation::Growth, 1},
    Scenario{"set_num_grow", Operation::SetNum, 1},
    Scenario{"set_num_shrink_reuse", Operation::SetNumShrink, 1},
    Scenario{"reset_reuse", Operation::ResetReuse, 1},
    Scenario{"remove_swap", Operation::RemoveSwap, 1},
    Scenario{"iterate", Operation::Iterate, 1},
    Scenario{"iterate_wide", Operation::IterateWide, 1},
    Scenario{"construct_views", Operation::Views, 1},
};

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
FORCENOINLINE auto execute(Owner& owner, Scenario const scenario, int32 const count) -> int64 {
    switch (scenario.operation) {
        case Operation::NaturalAppend:
        case Operation::ReservedAppend:
        case Operation::DefaultedAppend:
            for (int32 index{}; index < count; index += scenario.batch) {
                append(owner, std::min(scenario.batch, count - index), scenario.operation == Operation::DefaultedAppend);
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
auto run_once(Scenario const scenario, int32 const count, bool const timed) -> double {
    Owner owner;
    prepare(owner, scenario.operation, count);
    bool const iteration{scenario.operation == Operation::Iterate || scenario.operation == Operation::IterateWide};
    auto view{owner.get_view()};
    auto const begin{timed ? FPlatformTime::Cycles64() : 0};
    int64 observable{};
    if (iteration) {
        iterate(view, scenario.operation == Operation::IterateWide);
    } else {
        observable = execute(owner, scenario, count);
    }
    auto const end{timed ? FPlatformTime::Cycles64() : 0};
    REQUIRE(observable >= 0);
    if (iteration) {
        REQUIRE(owner.get_view().locations.zs[count - 1] == 1.5f);
        if (scenario.operation == Operation::IterateWide) {
            REQUIRE(owner.get_view().target_locations.zs[count - 1] == 3.f);
        }
    } else if (scenario.operation == Operation::RemoveSwap) {
        REQUIRE(owner.num() == count - count / 4);
    } else if (scenario.operation == Operation::Reserve) {
        REQUIRE(owner.num() == 0);
    } else {
        REQUIRE(owner.num() == count);
    }
    if (scenario.operation == Operation::Growth) {
        REQUIRE(owner.get_view().healths[count - 1] == count - 1);
    }
    return FPlatformTime::ToSeconds64(end - begin) * 1.e9;
}

auto counts() -> TArray<int32> {
    auto const args{get_benchmark_cli_args()};
    if (args.benchmark_entities) {
        REQUIRE(*args.benchmark_entities > 0);
        REQUIRE(*args.benchmark_entities <= 1048576);
        return {*args.benchmark_entities};
    }
    return {4096, 65536, 1048576};
}

struct Distribution {
    double median;
    double lower;
    double upper;
};

auto distribution(std::array<double, 31> samples) -> Distribution {
    std::ranges::sort(samples);
    return {samples[15], samples[3], samples[27]};
}

TEST_CASE("SandboxCore.SingleAllocation.BenchmarkCorrectness") {
    for (auto const scenario : scenarios) {
        CAPTURE(scenario.name);
        run_once<EntityData>(scenario, 129, false);
        run_once<SingleAllocationEntityData>(scenario, 129, false);
    }
}

TEST_CASE("SandboxCore.SingleAllocation.TimedComparison", "[benchmark]") {
    std::printf("SOA_TIMING,count,operation,baseline_median_ns,baseline_p10_ns,baseline_p90_ns,single_median_ns,single_p10_ns,single_p90_"
                "ns,baseline_over_single\n");
    for (int32 const count : counts()) {
        for (auto const scenario : scenarios) {
            for (int32 warmup{}; warmup < 3; ++warmup) {
                run_once<EntityData>(scenario, count, false);
                run_once<SingleAllocationEntityData>(scenario, count, false);
            }
            std::array<double, 31> baseline{};
            std::array<double, 31> single{};
            for (int32 sample{}; sample < 31; ++sample) {
                if (sample % 2 == 0) {
                    baseline[sample] = run_once<EntityData>(scenario, count, true);
                    single[sample] = run_once<SingleAllocationEntityData>(scenario, count, true);
                } else {
                    single[sample] = run_once<SingleAllocationEntityData>(scenario, count, true);
                    baseline[sample] = run_once<EntityData>(scenario, count, true);
                }
            }
            auto const a{distribution(baseline)};
            auto const b{distribution(single)};
            std::printf("SOA_TIMING,%d,%s,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.5f\n",
                        count,
                        scenario.name,
                        a.median,
                        a.lower,
                        a.upper,
                        b.median,
                        b.lower,
                        b.upper,
                        a.median / b.median);
            std::fflush(stdout);
        }
    }
}

struct AllocationSnapshot {
    SIZE_T requested{};
    SIZE_T usable{};
    SIZE_T live{};
    SIZE_T padding{};
    std::array<SIZE_T, 53> column_bytes{};
    int32 blocks{};
    int32 min_capacity{std::numeric_limits<int32>::max()};
    int32 max_capacity{};
};

auto snapshot(EntityData& owner, bool const query_allocator) -> AllocationSnapshot {
    AllocationSnapshot result;
    int32 leaf{};
    each_leaf(owner, [&](auto& column) {
        using Element = std::remove_cvref_t<decltype(column[0])>;
        auto const bytes{static_cast<SIZE_T>(column.Max()) * sizeof(Element)};
        result.column_bytes[leaf++] = bytes;
        result.requested += bytes;
        result.live += static_cast<SIZE_T>(column.Num()) * sizeof(Element);
        result.min_capacity = std::min(result.min_capacity, column.Max());
        result.max_capacity = std::max(result.max_capacity, column.Max());
        if (column.GetData() != nullptr) {
            ++result.blocks;
            if (query_allocator) {
                result.usable += FMemory::GetAllocSize(column.GetData());
            }
        }
    });
    return result;
}

auto snapshot(SingleAllocationEntityData& owner, bool const query_allocator) -> AllocationSnapshot {
    AllocationSnapshot result;
    result.requested = owner.allocated_bytes();
    result.column_bytes[0] = result.requested;
    result.min_capacity = owner.capacity();
    result.max_capacity = owner.capacity();
    SIZE_T row_bytes{};
    each_leaf(owner.get_view(), [&](auto column) {
        using Element = std::remove_cvref_t<decltype(column[0])>;
        row_bytes += sizeof(Element);
    });
    result.live = row_bytes * static_cast<SIZE_T>(owner.num());
    result.padding = result.requested - row_bytes * static_cast<SIZE_T>(owner.capacity());
    if (owner.capacity() > 0) {
        result.blocks = 1;
        if (query_allocator) {
            result.usable = FMemory::GetAllocSize(owner.get_view().entity_handles.GetData());
        }
    }
    return result;
}

template <typename Owner>
void allocation_diagnostics(char const* const name, int32 const count, bool const reserve_first) {
    Owner owner;
    auto previous{snapshot(owner, false)};
    SIZE_T requests{};
    SIZE_T peak_bound{};
    auto record = [&] {
        auto const current{snapshot(owner, false)};
        auto running_bytes{previous.requested};
        auto const leaf_count{current.column_bytes.size()};
        for (SIZE_T leaf{}; leaf < leaf_count; ++leaf) {
            auto const old_bytes{previous.column_bytes[leaf]};
            auto const new_bytes{current.column_bytes[leaf]};
            if (old_bytes != new_bytes) {
                ++requests;
                peak_bound = std::max(peak_bound, running_bytes + new_bytes);
                running_bytes += new_bytes - old_bytes;
            }
        }
        previous = current;
    };
    if (reserve_first) {
        owner.reserve(count);
        record();
    }
    for (int32 index{}; index < count; ++index) {
        owner.add_uninitialised(1);
        record();
    }
    auto const final{snapshot(owner, true)};
    std::printf("SOA_ALLOCATION,%s,%d,%d,%zu,%d,%zu,%zu,%zu,%zu,%zu,%zu,%d,%d,%zu\n",
                name,
                count,
                reserve_first ? 1 : 0,
                static_cast<std::size_t>(requests),
                final.blocks,
                static_cast<std::size_t>(final.requested),
                static_cast<std::size_t>(final.usable),
                static_cast<std::size_t>(final.live),
                static_cast<std::size_t>(final.requested - final.live - final.padding),
                static_cast<std::size_t>(final.padding),
                static_cast<std::size_t>(peak_bound),
                final.min_capacity,
                final.max_capacity,
                sizeof(Owner));
}

TEST_CASE("SandboxCore.SingleAllocation.AllocationDiagnostics") {
    std::printf("SOA_ALLOCATOR,%s\n", TCHAR_TO_UTF8(UE::Private::GMalloc->GetDescriptiveName()));
    std::printf("SOA_ALLOCATION,owner,count,reserved,allocation_requests,retained_blocks,requested_bytes,usable_bytes,live_bytes,row_slack_"
                "bytes,padding_bytes,peak_requested_bound,min_capacity,max_capacity,owner_bytes\n");
    for (int32 const count : counts()) {
        for (bool const reserve_first : {false, true}) {
            allocation_diagnostics<EntityData>("TArray", count, reserve_first);
            allocation_diagnostics<SingleAllocationEntityData>("Single", count, reserve_first);
        }
    }
}

}
