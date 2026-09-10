#include <SandboxCore/frame_array.h>
#include <SandboxCore/frame_memory_resource.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

#include <Async/ParallelFor.h>
#include <catch2/benchmark/catch_benchmark.hpp>

#include <array>
#include <atomic>
#include <string>

namespace ml::frame_memory_benchmarks {
inline constexpr int32 job_count{8};
inline constexpr int32 total_element_count{5000};
inline constexpr int32 timing_repeat_count{64};
inline constexpr SIZE_T frame_capacity_bytes{16 * 1024 * 1024};

struct FJobScratch {
    TArray<int32> indices;
    TArray<uint64> handles;
};

struct FMeasurement {
    uint64 checksum{};
    SIZE_T claimed_bytes{};
    SIZE_T payload_bytes{};
    SIZE_T padding_bytes{};
    uint64 root_claim_count{};
    uint64 local_allocation_count{};
    uint64 local_chunk_count{};
};

template <typename Function>
FORCENOINLINE auto repeat_for_timing(Function&& function) -> uint64 {
    uint64 checksum{};
    for (int32 repeat{}; repeat < timing_repeat_count; ++repeat) {
        checksum += function();
    }
    return checksum;
}

template <typename Indices, typename Handles>
FORCENOINLINE auto populate(Indices& indices, Handles& handles, int32 const begin, int32 const end, bool const reserve) -> uint64 {
    if (reserve) {
        indices.reserve(end - begin);
        handles.reserve(end - begin);
    }

    uint64 checksum{};
    for (int32 i{begin}; i < end; ++i) {
        if ((i & 1) != 0) {
            continue;
        }
        indices.add(i);
        handles.add(static_cast<uint64>(i) * 17);
        checksum += handles[handles.num() - 1];
    }
    return checksum;
}

template <typename Indices, typename Handles>
FORCENOINLINE auto populate_turret_scratch(Indices& indices, Handles& handles) -> uint64 {
    indices.reserve(total_element_count);
    for (int32 i{}; i < total_element_count; ++i) {
        if ((i & 1) == 0) {
            indices.add(i);
        }
    }

    handles.reserve(indices.num());
    uint64 checksum{};
    for (int32 const index : indices) {
        handles.add(static_cast<uint64>(index) * 17);
        checksum += handles[handles.num() - 1];
    }
    return checksum;
}

FORCENOINLINE auto run_persistent(std::array<FJobScratch, job_count>& scratch, bool const reserve) -> FMeasurement {
    std::atomic<uint64> checksum{};
    ParallelFor(job_count, [&scratch, &checksum, reserve](int32 const job_index) {
        auto& job{scratch[job_index]};
        job.indices.Reset();
        job.handles.Reset();
        auto const begin{job_index * total_element_count / job_count};
        auto const end{(job_index + 1) * total_element_count / job_count};

        if (reserve) {
            job.indices.Reserve(end - begin);
            job.handles.Reserve(end - begin);
        }
        uint64 local_checksum{};
        for (int32 i{begin}; i < end; ++i) {
            if ((i & 1) != 0) {
                continue;
            }
            job.indices.Add(i);
            job.handles.Add(static_cast<uint64>(i) * 17);
            local_checksum += job.handles.Last();
        }
        checksum.fetch_add(local_checksum, std::memory_order_relaxed);
    });

    SIZE_T allocated_bytes{};
    for (auto const& job : scratch) {
        allocated_bytes += job.indices.GetAllocatedSize() + job.handles.GetAllocatedSize();
    }
    return {.checksum = checksum.load(std::memory_order_relaxed), .claimed_bytes = allocated_bytes, .payload_bytes = allocated_bytes};
}

FORCENOINLINE auto run_fresh_tarray(bool const reserve) -> FMeasurement {
    std::atomic<uint64> checksum{};
    std::atomic<SIZE_T> allocated_bytes{};
    ParallelFor(job_count, [&checksum, &allocated_bytes, reserve](int32 const job_index) {
        auto const begin{job_index * total_element_count / job_count};
        auto const end{(job_index + 1) * total_element_count / job_count};
        TArray<int32> indices;
        TArray<uint64> handles;
        if (reserve) {
            indices.Reserve(end - begin);
            handles.Reserve(end - begin);
        }

        uint64 local_checksum{};
        for (int32 i{begin}; i < end; ++i) {
            if ((i & 1) != 0) {
                continue;
            }
            indices.Add(i);
            handles.Add(static_cast<uint64>(i) * 17);
            local_checksum += handles.Last();
        }
        checksum.fetch_add(local_checksum, std::memory_order_relaxed);
        allocated_bytes.fetch_add(indices.GetAllocatedSize() + handles.GetAllocatedSize(), std::memory_order_relaxed);
    });
    return {.checksum = checksum.load(std::memory_order_relaxed),
            .claimed_bytes = allocated_bytes.load(std::memory_order_relaxed),
            .payload_bytes = allocated_bytes.load(std::memory_order_relaxed)};
}

FORCENOINLINE auto run_direct(FFrameMemoryResource& root, bool const reserve) -> FMeasurement {
    std::atomic<uint64> checksum{};
    ParallelFor(job_count, [&root, &checksum, reserve](int32 const job_index) {
        auto const begin{job_index * total_element_count / job_count};
        auto const end{(job_index + 1) * total_element_count / job_count};
        TFrameArray<int32> indices{&root};
        TFrameArray<uint64> handles{&root};
        checksum.fetch_add(populate(indices, handles, begin, end, reserve), std::memory_order_relaxed);
    });

    auto const stats{root.get_stats()};
    FMeasurement const result{
        .checksum = checksum.load(std::memory_order_relaxed),
        .claimed_bytes = stats.current_claimed_bytes,
        .payload_bytes = stats.current_payload_bytes,
        .padding_bytes = stats.current_padding_bytes,
        .root_claim_count = stats.current_root_claim_count,
    };
    root.reset();
    return result;
}

FORCENOINLINE auto run_local(FFrameMemoryResource& root, bool const reserve) -> FMeasurement {
    std::atomic<uint64> checksum{};
    std::array<FLocalFrameMemoryStats, job_count> local_stats{};
    ParallelFor(job_count, [&root, &checksum, &local_stats, reserve](int32 const job_index) {
        auto const begin{job_index * total_element_count / job_count};
        auto const end{(job_index + 1) * total_element_count / job_count};
        auto const count{end - begin};
        auto const chunk_bytes{static_cast<SIZE_T>(count) * (sizeof(int32) + sizeof(uint64)) + alignof(uint64) - 1};
        FLocalFrameMemoryResource local{&root, chunk_bytes};
        TFrameArray<int32> indices{&local};
        TFrameArray<uint64> handles{&local};
        checksum.fetch_add(populate(indices, handles, begin, end, reserve), std::memory_order_relaxed);
        local_stats[job_index] = local.get_stats();
    });

    auto const stats{root.get_stats()};
    FMeasurement result{
        .checksum = checksum.load(std::memory_order_relaxed),
        .claimed_bytes = stats.current_claimed_bytes,
        .payload_bytes = stats.current_payload_bytes,
        .padding_bytes = stats.current_padding_bytes,
        .root_claim_count = stats.current_root_claim_count,
    };
    for (auto const local : local_stats) {
        result.local_allocation_count += local.allocation_count;
        result.local_chunk_count += local.upstream_claim_count;
    }
    root.reset();
    return result;
}

FORCENOINLINE auto run_persistent_serial(FJobScratch& scratch) -> FMeasurement {
    scratch.indices.Reset();
    scratch.handles.Reset();
    scratch.indices.Reserve(total_element_count);
    for (int32 i{}; i < total_element_count; ++i) {
        if ((i & 1) == 0) {
            scratch.indices.Add(i);
        }
    }

    scratch.handles.Reserve(scratch.indices.Num());
    uint64 checksum{};
    for (int32 const index : scratch.indices) {
        scratch.handles.Add(static_cast<uint64>(index) * 17);
        checksum += scratch.handles.Last();
    }
    auto const allocated_bytes{scratch.indices.GetAllocatedSize() + scratch.handles.GetAllocatedSize()};
    return {.checksum = checksum, .claimed_bytes = allocated_bytes, .payload_bytes = allocated_bytes};
}

FORCENOINLINE auto run_direct_serial(FFrameMemoryResource& root) -> FMeasurement {
    uint64 checksum{};
    {
        TFrameArray<int32> indices{&root};
        TFrameArray<uint64> handles{&root};
        checksum = populate_turret_scratch(indices, handles);
    }
    auto const stats{root.get_stats()};
    FMeasurement const result{.checksum = checksum,
                              .claimed_bytes = stats.current_claimed_bytes,
                              .payload_bytes = stats.current_payload_bytes,
                              .padding_bytes = stats.current_padding_bytes,
                              .root_claim_count = stats.current_root_claim_count};
    root.reset();
    return result;
}

FORCENOINLINE auto run_local_serial(FFrameMemoryResource& root) -> FMeasurement {
    auto const chunk_bytes{static_cast<SIZE_T>(total_element_count) * sizeof(int32) +
                           static_cast<SIZE_T>(total_element_count / 2) * sizeof(uint64) + alignof(uint64) - 1};
    FMeasurement result;
    {
        FLocalFrameMemoryResource local{&root, chunk_bytes};
        TFrameArray<int32> indices{&local};
        TFrameArray<uint64> handles{&local};
        result.checksum = populate_turret_scratch(indices, handles);
        auto const local_stats{local.get_stats()};
        result.local_allocation_count = local_stats.allocation_count;
        result.local_chunk_count = local_stats.upstream_claim_count;
    }
    auto const stats{root.get_stats()};
    result.claimed_bytes = stats.current_claimed_bytes;
    result.payload_bytes = stats.current_payload_bytes;
    result.padding_bytes = stats.current_padding_bytes;
    result.root_claim_count = stats.current_root_claim_count;
    root.reset();
    return result;
}

TEST_CASE("SandboxCore.FrameMemory.BenchmarkCorrectness") {
    std::array<FJobScratch, job_count> persistent_scratch;
    FFrameMemoryResource direct_root{frame_capacity_bytes};
    FFrameMemoryResource local_root{frame_capacity_bytes};

    auto const persistent{run_persistent(persistent_scratch, true)};
    auto const direct{run_direct(direct_root, true)};
    auto const local{run_local(local_root, true)};

    REQUIRE(persistent.checksum == direct.checksum);
    REQUIRE(direct.checksum == local.checksum);
    REQUIRE(direct.root_claim_count == job_count * 2);
    REQUIRE(local.local_allocation_count == job_count * 2);
    REQUIRE(local.local_chunk_count <= local.local_allocation_count);
    REQUIRE(local.root_claim_count == local.local_chunk_count);
    REQUIRE(direct_root.get_stats().outstanding_allocation_count == 0);
    REQUIRE(local_root.get_stats().outstanding_allocation_count == 0);
}

TEST_CASE("SandboxCore.FrameMemory.Timing", "[benchmark]") {
    std::array<FJobScratch, job_count> persistent_scratch;
    FJobScratch persistent_serial_scratch;
    FFrameMemoryResource direct_root{frame_capacity_bytes};
    FFrameMemoryResource local_root{frame_capacity_bytes};

    // Warm the persistent baseline so its measured behavior matches the existing per-frame reuse.
    run_persistent(persistent_scratch, true);
    run_persistent_serial(persistent_serial_scratch);

    BENCHMARK("TurretScratch,5000,serial,reserved,persistent_TArray_reuse,x64") {
        return repeat_for_timing([&] { return run_persistent_serial(persistent_serial_scratch).checksum; });
    };
    BENCHMARK("TurretScratch,5000,serial,reserved,direct_root,x64") {
        return repeat_for_timing([&] { return run_direct_serial(direct_root).checksum; });
    };
    BENCHMARK("TurretScratch,5000,serial,reserved,local_monotonic,x64") {
        return repeat_for_timing([&] { return run_local_serial(local_root).checksum; });
    };

    BENCHMARK("FrameMemory,5000,8,reserved,persistent_TArray_reuse,x64") {
        return repeat_for_timing([&] { return run_persistent(persistent_scratch, true).checksum; });
    };
    BENCHMARK("FrameMemory,5000,8,reserved,direct_root,x64") {
        return repeat_for_timing([&] { return run_direct(direct_root, true).checksum; });
    };
    BENCHMARK("FrameMemory,5000,8,reserved,local_monotonic,x64") {
        return repeat_for_timing([&] { return run_local(local_root, true).checksum; });
    };

    BENCHMARK("FrameMemory,5000,8,growth,fresh_TArray,x64") {
        return repeat_for_timing([&] { return run_fresh_tarray(false).checksum; });
    };
    BENCHMARK("FrameMemory,5000,8,growth,direct_root,x64") {
        return repeat_for_timing([&] { return run_direct(direct_root, false).checksum; });
    };
    BENCHMARK("FrameMemory,5000,8,growth,local_monotonic,x64") {
        return repeat_for_timing([&] { return run_local(local_root, false).checksum; });
    };
}

TEST_CASE("SandboxCore.FrameMemory.TransientMemory", "[benchmark]") {
    {
        FJobScratch persistent_scratch;
        FFrameMemoryResource direct_root{frame_capacity_bytes};
        FFrameMemoryResource local_root{frame_capacity_bytes};
        auto const persistent{run_persistent_serial(persistent_scratch)};
        auto const direct{run_direct_serial(direct_root)};
        auto const local{run_local_serial(local_root)};

        WARN("TurretScratch persistent_bytes=" << persistent.claimed_bytes << " direct_claimed=" << direct.claimed_bytes
                                               << " direct_payload=" << direct.payload_bytes << " direct_padding=" << direct.padding_bytes
                                               << " direct_root_claims=" << direct.root_claim_count
                                               << " local_claimed=" << local.claimed_bytes << " local_payload=" << local.payload_bytes
                                               << " local_padding=" << local.padding_bytes << " local_allocations="
                                               << local.local_allocation_count << " local_chunks=" << local.local_chunk_count);
    }

    for (bool const reserve : {true, false}) {
        std::array<FJobScratch, job_count> persistent_scratch;
        FFrameMemoryResource direct_root{frame_capacity_bytes};
        FFrameMemoryResource local_root{frame_capacity_bytes};
        auto const persistent{run_persistent(persistent_scratch, reserve)};
        auto const direct{run_direct(direct_root, reserve)};
        auto const local{run_local(local_root, reserve)};

        WARN("FrameMemory reserve=" << reserve << " persistent_bytes=" << persistent.claimed_bytes
                                    << " direct_claimed=" << direct.claimed_bytes << " direct_payload=" << direct.payload_bytes
                                    << " direct_padding=" << direct.padding_bytes << " direct_root_claims=" << direct.root_claim_count
                                    << " local_claimed=" << local.claimed_bytes << " local_payload=" << local.payload_bytes
                                    << " local_padding=" << local.padding_bytes << " local_allocations=" << local.local_allocation_count
                                    << " local_chunks=" << local.local_chunk_count);
    }
}
}
