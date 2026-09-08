#include <algorithm>
#include <array>
#include <SbxCoreExperiments/soa_test_support.h>
#include <SbxCoreExperiments/soa_types.h>
#include <type_traits>
#include "single_allocation_soa_benchmark_counts.h"

#include <catch2/benchmark/detail/catch_benchmark_stats.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>

#include <cstdio>

namespace ml::single_allocation_benchmarks {
using namespace single_allocation_experiment;
class BenchmarkCsvListener : public Catch::EventListenerBase {
  public:
    using EventListenerBase::EventListenerBase;

    void benchmarkEnded(Catch::BenchmarkStats<> const& stats) override {
        if (!stats.info.name.starts_with("SOA,")) {
            return;
        }
        std::printf("\nSOA_CATCH,%s,%.9g,%.9g,%.9g,%.9g,%d,%u\n",
                    stats.info.name.c_str() + 4,
                    stats.mean.point.count(),
                    stats.mean.lower_bound.count(),
                    stats.mean.upper_bound.count(),
                    stats.mean.confidence_interval,
                    stats.info.iterations,
                    stats.info.samples);
        std::fflush(stdout);
    }
};

CATCH_REGISTER_LISTENER(BenchmarkCsvListener)

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

auto snapshot(FMemorySingleEntityData& owner, bool const query_allocator) -> AllocationSnapshot {
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
            allocation_diagnostics<FMemorySingleEntityData>("Single", count, reserve_first);
        }
    }
}

}
