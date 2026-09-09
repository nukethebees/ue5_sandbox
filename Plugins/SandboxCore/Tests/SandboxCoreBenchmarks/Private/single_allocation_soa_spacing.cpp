#include "single_allocation_soa_spacing_support.h"

namespace ml::soa_spacing_experiment {
using namespace single_allocation_experiment;
TEST_CASE("SandboxCore.SingleAllocation.Spacing.Measure", "[benchmark][.]") {
    auto const capacity{parameter(TEXT("SOA_SPACING_CAPACITY"))};
    auto const live{parameter(TEXT("SOA_SPACING_LIVE"))};
    auto const gap{parameter(TEXT("SOA_SPACING_GAP"))};
    auto const owner{parameter(TEXT("SOA_SPACING_OWNER"))};
    auto const wide{parameter(TEXT("SOA_SPACING_WIDE")) != 0};
    REQUIRE(capacity >= live);
    REQUIRE(live > 0);
    REQUIRE(capacity % 64 == 0);
    REQUIRE(gap >= 0);
    if (owner == 0) {
        SpacedOwner storage{capacity, live, static_cast<SIZE_T>(gap)};
        describe(storage.view, storage.data, capacity, storage.bytes);
        measure(storage.view, wide);
        storage.check_gaps();
    } else if (owner == 1) {
        SingleAllocationEntityData storage;
        storage.reserve(capacity);
        storage.add_defaulted(live);
        auto view{storage.get_view().columns()};
        single_allocation_benchmarks::initialise(view);
        describe(view, reinterpret_cast<std::byte*>(view.entity_handles.GetData()), storage.capacity(), storage.allocated_bytes());
        measure(view, wide);
    } else {
        EntityData storage;
        storage.reserve(capacity);
        storage.add_defaulted(live);
        auto view{storage.get_view()};
        single_allocation_benchmarks::initialise(view);
        std::vector<int32> capacities;
        SIZE_T bytes{};
        each_leaf(storage, [&](auto const& column) {
            capacities.push_back(column.Max());
            bytes += static_cast<SIZE_T>(column.Max()) * sizeof(column[0]);
        });
        describe(view, nullptr, capacity, bytes, capacities);
        measure(view, wide);
    }
}
TEST_CASE("SandboxCore.SingleAllocation.Spacing.Correctness") {
    for (auto const capacity : {65536, 65600, 75008, 100032, 131072}) {
        SingleAllocationEntityData reference;
        reference.reserve(capacity);
        reference.add_defaulted(17);
        auto expected{reference.get_view().columns()};
        single_allocation_benchmarks::initialise(expected);
        SpacedOwner zero{capacity, 17, 192};
        REQUIRE(zero.bytes == reference.allocated_bytes());
        auto const base{reinterpret_cast<UPTRINT>(expected.entity_handles.GetData())};
        SIZE_T index{};
        each_leaf(expected,
                  [&](auto column) { REQUIRE(reinterpret_cast<UPTRINT>(column.GetData()) - base == zero.columns[index++].offset); });
        for (bool wide : {false, true}) {
            for (SIZE_T gap : {0, 64, 128, 192, 256, 384, 512, 768, 1024}) {
                SpacedOwner candidate{capacity, 17, gap};
                SpacedOwner control{capacity, 17, 0};
                single_allocation_benchmarks::iterate(candidate.view, wide);
                single_allocation_benchmarks::iterate(control.view, wide);
                index = 0;
                each_leaf(candidate.view, [&](auto column) {
                    REQUIRE(FMemory::Memcmp(column.GetData(), control.data + control.columns[index++].offset, 17 * sizeof(column[0])) == 0);
                });
                REQUIRE(candidate.view.locations.zs[16] == 1.5f);
                if (wide) {
                    REQUIRE(candidate.view.target_locations.zs[16] == 3.f);
                }
                candidate.check_gaps();
            }
        }
    }
}
}
