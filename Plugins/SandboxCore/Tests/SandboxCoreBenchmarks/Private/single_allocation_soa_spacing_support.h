#pragma once

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/benchmark/catch_optimizer.hpp>
#include <cstdio>
#include <HAL/PlatformMisc.h>
#include <SbxCoreExperiments/soa_spacing_shapes.h>
#include <SbxCoreExperiments/soa_test_support.h>
#include <SbxCoreExperiments/soa_types.h>
#include <vector>
#include "TestHarness.h"

namespace ml::single_allocation_benchmarks {
void iterate(single_allocation_experiment::EntityDataView view, bool wide);
void initialise(single_allocation_experiment::EntityDataView view);
}
namespace ml::soa_spacing_experiment {
using namespace single_allocation_experiment;
inline void initialise(EntityDataView view) {
    single_allocation_benchmarks::initialise(view);
}
inline void iterate(EntityDataView view, bool wide) {
    single_allocation_benchmarks::iterate(view, wide);
}
void initialise(SpacingDoublesView view);
void initialise(SpacingMixedWidthsView view);
void initialise(SpacingAlignedView view);
inline void initialise(AlignmentDataView) {}
void iterate(SpacingDoublesView view, bool wide);
void iterate(SpacingMixedWidthsView view, bool wide);
void iterate(SpacingAlignedView view, bool wide);

struct Column {
    SIZE_T offset{}, bytes{}, element_size{}, alignment{};
};
template <typename View = EntityDataView>
struct SpacedOwner {
    View view{};
    std::vector<Column> columns;
    std::byte* data{};
    SIZE_T bytes{};
    SIZE_T allocation_alignment{64};
    SpacedOwner(int32 capacity, int32 live, SIZE_T gap) {
        each_leaf(view, [&](auto& column) {
            using Element = std::remove_cvref_t<decltype(column[0])>;
            static_assert(soa_storage::supported_leaf<Element>);
            auto const alignment{std::max(SIZE_T{64}, alignof(Element))};
            auto const cursor{bytes + (columns.empty() ? 0 : gap)};
            auto const offset{(cursor + alignment - 1) & ~(alignment - 1)};
            auto const extent{static_cast<SIZE_T>(capacity) * sizeof(Element)};
            columns.push_back({offset, extent, sizeof(Element), alignment});
            bytes = offset + extent;
            allocation_alignment = std::max(allocation_alignment, alignment);
        });
        data = soa_storage::MimallocStorageAllocator::allocate(bytes, static_cast<uint32>(allocation_alignment));
        FMemory::Memset(data, 0xcd, bytes);
        SIZE_T index{};
        each_leaf(view, [&](auto& column) {
            using Element = std::remove_cvref_t<decltype(column[0])>;
            auto* values{reinterpret_cast<Element*>(data + columns[index++].offset)};
            for (int32 row{}; row < live; ++row) {
                ::new (values + row) Element{};
            }
            column = {values, live};
        });
        initialise(view);
    }
    ~SpacedOwner() { soa_storage::MimallocStorageAllocator::free(data); }
    SpacedOwner(SpacedOwner const&) = delete;
    auto operator=(SpacedOwner const&) -> SpacedOwner& = delete;
    void check_gaps() const {
        SIZE_T end{};
        REQUIRE(reinterpret_cast<UPTRINT>(data) % allocation_alignment == 0);
        for (auto const column : columns) {
            REQUIRE(column.offset >= end);
            REQUIRE(reinterpret_cast<UPTRINT>(data + column.offset) % column.alignment == 0);
            REQUIRE(column.offset + column.bytes <= bytes);
            for (auto index{end}; index < column.offset; ++index) {
                REQUIRE(std::to_integer<unsigned>(data[index]) == 0xcd);
            }
            end = column.offset + column.bytes;
        }
    }
};

inline auto parameter(TCHAR const* name) -> int32 {
    return FCString::Atoi(*FPlatformMisc::GetEnvironmentVariable(name));
}
template <typename View>
void measure(View view, bool wide) {
    BENCHMARK(wide ? "SOA_SPACING,wide" : "SOA_SPACING,narrow") {
        iterate(view, wide);
        Catch::Benchmark::deoptimize_value(view);
        return view.num();
    };
}
template <typename View>
void describe(View view, std::byte const* base, int32 capacity, SIZE_T bytes, std::vector<int32> const& column_capacities = {}) {
    std::printf("SPACING_ALLOCATION,%zu,%zu,%d\n",
                static_cast<std::size_t>(reinterpret_cast<UPTRINT>(base)),
                static_cast<std::size_t>(bytes),
                capacity);
    SIZE_T ordinal{};
    each_leaf(view, [&](auto column) {
        using Element = std::remove_cvref_t<decltype(column[0])>;
        auto const address{reinterpret_cast<UPTRINT>(column.GetData())};
        auto const column_capacity{column_capacities.empty() ? capacity : column_capacities[ordinal]};
        std::printf("SPACING_COLUMN,%zu,%zu,%zu,%zu,%zu,%d\n",
                    static_cast<std::size_t>(ordinal),
                    static_cast<std::size_t>(address),
                    static_cast<std::size_t>(sizeof(Element)),
                    static_cast<std::size_t>(base ? std::max(SIZE_T{64}, alignof(Element)) : alignof(Element)),
                    static_cast<std::size_t>(base ? address - reinterpret_cast<UPTRINT>(base) : 0),
                    column_capacity);
        ++ordinal;
    });
}
}
