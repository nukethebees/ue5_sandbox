#include <benchmark/benchmark.h>
#include <native_soa_types.h>

#include <malloc.h>
#include <string>
#include <vector>

#if NATIVE_SOA_MIMALLOC
#include <mimalloc.h>
#endif

namespace ml::native_soa_reserve_matrix {
using namespace native_experiment;

void counters(benchmark::State& state) {
    auto const rows{state.range(0)};
    auto const owners{state.range(1)};
    state.counters["rows"] = static_cast<double>(rows);
    state.counters["owners"] = static_cast<double>(owners);
    state.SetItemsProcessed(state.iterations() * owners);
}

template <typename Owner>
void reserve(benchmark::State& state) {
    auto const rows{static_cast<std::int32_t>(state.range(0))};
    auto const count{static_cast<std::size_t>(state.range(1))};
    for (auto _ : state) {
        std::vector<Owner> owners(count);
        for (auto& owner : owners) {
            owner.reserve(rows);
        }
        benchmark::DoNotOptimize(owners.data());
        benchmark::ClobberMemory();
    }
    counters(state);
}

template <bool Realloc>
void raw_reserve(benchmark::State& state) {
    auto const rows{native_soa::rounded_capacity(state.range(0),
                                                 SingleAllocationEntityData::capacity_block_bound)};
    auto const bytes{SingleAllocationEntityData::layout_bytes(static_cast<std::size_t>(rows / 64))};
    auto constexpr alignment{SingleAllocationEntityData::allocation_alignment};
    auto const count{static_cast<std::size_t>(state.range(1))};
    for (auto _ : state) {
        std::vector<void*> allocations(count);
        for (auto& allocation : allocations) {
#if NATIVE_SOA_MIMALLOC
            allocation = Realloc ? mi_realloc_aligned(nullptr, bytes, alignment)
                                 : mi_malloc_aligned(bytes, alignment);
#else
            allocation = Realloc ? _aligned_realloc(nullptr, bytes, alignment)
                                 : _aligned_malloc(bytes, alignment);
#endif
            if (allocation == nullptr) {
                throw std::bad_alloc{};
            }
        }
        benchmark::DoNotOptimize(allocations.data());
        benchmark::ClobberMemory();
        for (auto* const allocation : allocations) {
#if NATIVE_SOA_MIMALLOC
            mi_free(allocation);
#else
            _aligned_free(allocation);
#endif
        }
    }
    counters(state);
}

inline auto const registered{[] {
#if NATIVE_SOA_MIMALLOC
    benchmark::AddCustomContext(
        "allocator", "mimalloc (explicit SoA allocators; standard outer vector and harness)");
    benchmark::AddCustomContext("mimalloc_version", std::to_string(mi_version()));
#else
    benchmark::AddCustomContext("allocator", "standard C++ allocator / Windows aligned CRT");
#endif
    benchmark::AddCustomContext(
        "measurement",
        "Retain complete batch; includes outer vector and all cleanup; no explicit row writes");
    auto add = [](char const* name, auto function) {
        auto const benchmark_name{std::string{name} + "/reserve_batch"};
        auto* const entry{benchmark::RegisterBenchmark(benchmark_name.c_str(), function)};
        for (auto const rows : {4096, 65536, 1048576}) {
            for (auto const owners : {1, 2, 4, 8, 16, 32, 64, 128, 200, 256, 512}) {
                entry->Args({rows, owners});
            }
        }
        entry->UseRealTime();
    };
    add("Vector", reserve<EntityData>);
    add("Single", reserve<SingleAllocationEntityData>);
    add("RawMalloc", raw_reserve<false>);
    add("RawRealloc", raw_reserve<true>);
    return true;
}()};
}
