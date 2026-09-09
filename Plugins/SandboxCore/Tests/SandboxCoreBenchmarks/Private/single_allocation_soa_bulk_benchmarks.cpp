#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/benchmark/catch_optimizer.hpp>
#include <SbxCoreExperiments/soa_test_support.h>
#include <SbxCoreExperiments/soa_types.h>
#include <string>
#include "TestHarness.h"

namespace ml::single_allocation_bulk_benchmarks {
using namespace single_allocation_experiment;
inline constexpr int32 row_count{65536};

template <typename Owner>
void append_batches(Owner& destination, Owner const& source, int32 batch) {
    auto const source_view{source.get_view()};
    for (int32 first{}; first < row_count; first += batch) {
        destination.append_from(source_view.slice(first, batch));
    }
    Catch::Benchmark::deoptimize_value(destination);
}

template <typename Owner>
void benchmark_append(bool reserved, int32 batch, char const* label) {
    Owner source;
    source.add_defaulted(row_count);
    auto const name{std::string{"SOA,65536,append_from_"} + (reserved ? "reserved_" : "natural_") + std::to_string(batch) + "," + label};
    BENCHMARK_ADVANCED(std::string{name})(Catch::Benchmark::Chronometer meter) {
        if (reserved) {
            Owner destination;
            destination.reserve(row_count);
            meter.measure([&] {
                destination.reset();
                append_batches(destination, source, batch);
                return destination.num();
            });
        } else {
            meter.measure([&] {
                Owner destination;
                append_batches(destination, source, batch);
                return destination.num();
            });
        }
    };
}

template <typename Owner>
void remove_indices(Owner& owner, TConstArrayView<int32> indices) {
    if constexpr (requires { owner.remove_at_swap(indices); }) {
        owner.remove_at_swap(indices);
    } else {
        auto const columns{owner.get_view()};
        soa_storage_detail::for_each_removal_run(owner.num(),
                                                 {indices.GetData(), static_cast<SIZE_T>(indices.Num())},
                                                 soa_storage::require,
                                                 [&](int32 destination, int32 source, int32 count) {
                                                     each_leaf(columns, [&](auto column) {
                                                         FMemory::Memcpy(column.GetData() + destination,
                                                                         column.GetData() + source,
                                                                         static_cast<SIZE_T>(count) * sizeof(column[0]));
                                                     });
                                                 });
        owner.set_num(owner.num() - indices.Num(), EAllowShrinking::No);
    }
}

template <typename Owner>
void benchmark_remove(bool scattered, char const* label) {
    Owner owner;
    owner.add_defaulted(row_count);
    TArray<int32> indices;
    for (int32 row{row_count / 4 - 1}; row >= 0; --row) {
        indices.Add(scattered ? row * 4 : row);
    }
    auto const name{std::string{"SOA,65536,remove_indices_"} + (scattered ? "scattered," : "clustered,") + label};
    BENCHMARK_ADVANCED(std::string{name})(Catch::Benchmark::Chronometer meter) {
        meter.measure([&] {
            owner.set_num(row_count, EAllowShrinking::No);
            remove_indices(owner, indices);
            Catch::Benchmark::deoptimize_value(owner);
            return owner.num();
        });
    };
}

FORCENOINLINE int32 consume_size(EntityDataConstView view) {
    return view.num();
}
FORCENOINLINE int32 consume_size(SingleAllocationEntityData::ConstView view) {
    return view.num();
}

template <typename Owner>
void benchmark_views(bool materialize, char const* label) {
    Owner owner;
    owner.add_defaulted(row_count);
    auto const name{std::string{"SOA,65536,"} + (materialize ? "materialize_columns," : "view_handles,") + label};
    BENCHMARK(std::string{name}) {
        int64 result{};
        for (int32 iteration{}; iteration < 4096; ++iteration) {
            if (materialize) {
                result += consume_size(array_columns(owner.get_const_view()));
            } else {
                result += consume_size(owner.get_const_view());
            }
        }
        return result;
    };
}

#define SOA_APPEND_CASE(mode, reserved, batch)                                                      \
    TEST_CASE("SandboxCore.SingleAllocation.Timing.append_from_" #mode "_" #batch, "[benchmark]") { \
        benchmark_append<EntityData>(reserved, batch, "TArray");                                    \
        benchmark_append<SingleAllocationEntityData>(reserved, batch, "SingleMimalloc");            \
    }
SOA_APPEND_CASE(natural, false, 64)
SOA_APPEND_CASE(natural, false, 65536)
SOA_APPEND_CASE(reserved, true, 64)
SOA_APPEND_CASE(reserved, true, 65536)
#undef SOA_APPEND_CASE

TEST_CASE("SandboxCore.SingleAllocation.Timing.remove_indices_clustered", "[benchmark]") {
    benchmark_remove<EntityData>(false, "TArray");
    benchmark_remove<SingleAllocationEntityData>(false, "SingleMimalloc");
}
TEST_CASE("SandboxCore.SingleAllocation.Timing.remove_indices_scattered", "[benchmark]") {
    benchmark_remove<EntityData>(true, "TArray");
    benchmark_remove<SingleAllocationEntityData>(true, "SingleMimalloc");
}
TEST_CASE("SandboxCore.SingleAllocation.Timing.view_handles", "[benchmark]") {
    benchmark_views<EntityData>(false, "TArray");
    benchmark_views<SingleAllocationEntityData>(false, "SingleMimalloc");
}
TEST_CASE("SandboxCore.SingleAllocation.Timing.materialize_columns", "[benchmark]") {
    benchmark_views<EntityData>(true, "TArray");
    benchmark_views<SingleAllocationEntityData>(true, "SingleMimalloc");
}

TEST_CASE("SandboxCore.SingleAllocation.BulkBenchmarkCorrectness", "[benchmark]") {
    EntityData baseline;
    SingleAllocationEntityData single;
    baseline.add_defaulted(row_count);
    single.append_from(single);
    SingleAllocationEntityData source;
    source.add_defaulted(row_count);
    for (int32 row{}; row < row_count; ++row) {
        baseline.get_view().healths[row] = row;
        source.get_view().healths()[row] = row;
    }
    append_batches(single, source, 64);
    REQUIRE(single.get_view().healths()[row_count - 1] == row_count - 1);
    EntityData copied;
    append_batches(copied, baseline, 64);
    REQUIRE(copied.num() == row_count);
    REQUIRE(copied.get_view().healths[row_count - 1] == row_count - 1);
    TArray<int32> indices;
    for (int32 row{row_count / 4 - 1}; row >= 0; --row) {
        indices.Add(row * 4);
    }
    remove_indices(baseline, indices);
    remove_indices(single, indices);
    REQUIRE(single.num() == baseline.num());
    for (int32 row{}; row < single.num(); ++row) {
        REQUIRE(single.get_view().healths()[row] == baseline.get_view().healths[row]);
    }
}
}
