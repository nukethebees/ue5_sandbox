#include <gtest/gtest.h>
#include <native_soa_types.h>

namespace ml::native_soa_tests {
using namespace native_experiment;

static_assert(!native_soa::supported_leaf<std::string>);
static_assert(!std::is_copy_constructible_v<SingleAllocationEntityData>);
static_assert(
    std::is_same_v<decltype(std::declval<SingleAllocationEntityData const&>().get_view().healths),
                   std::span<std::int32_t const>>);

TEST(NativeSoa, LayoutGrowthAndMoves) {
    AlignmentData vectors;
    vectors.set_num(129);
    vectors.each_column([](auto const& column) {
        using Element = typename std::remove_cvref_t<decltype(column)>::value_type;
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(column.data()) % alignof(Element), 0u);
#if NATIVE_SOA_MIMALLOC
        EXPECT_TRUE(mi_is_in_heap_region(column.data()));
#endif
    });
    SingleAllocationAlignmentData owner;
    EXPECT_EQ(owner.num(), 0);
    EXPECT_EQ(owner.capacity(), 0);
    EXPECT_EQ(owner.get_view().bytes.data(), nullptr);
    for (std::int32_t const count : {1, 63, 64, 65, 127, 128, 129, 4097, 65537}) {
        auto const previous{owner.num()};
        owner.set_num(count);
        EXPECT_EQ(owner.num(), count);
        EXPECT_GE(owner.capacity(), count);
        EXPECT_EQ(owner.capacity() % 64, 0);
        auto const view{owner.get_view()};
#if NATIVE_SOA_MIMALLOC
        EXPECT_TRUE(mi_is_in_heap_region(view.bytes.data()));
#endif
        EXPECT_EQ(view.aligned32.back().value, 32);
        EXPECT_EQ(view.aligned64.back().value, 64);
        EXPECT_EQ(view.aligned256.back().value, 256);
        EXPECT_EQ(view.handles.back().index, -1);
        EXPECT_EQ(view.nested.xs.back(), 0.f);
        for (std::int32_t i{}; i < previous; ++i) {
            EXPECT_EQ(view.nested.ys[i], static_cast<float>(i));
        }
        for (std::int32_t i{}; i < count; ++i) {
            view.nested.ys[i] = static_cast<float>(i);
        }
        std::uintptr_t previous_end{};
        auto const base{reinterpret_cast<std::uintptr_t>(view.bytes.data())};
        view.each_column([&](auto column) {
            using T = typename decltype(column)::value_type;
            auto const address{reinterpret_cast<std::uintptr_t>(column.data())};
            EXPECT_EQ(address % std::max(std::size_t{64}, alignof(T)), 0);
            EXPECT_GE(address, previous_end);
            previous_end = address + static_cast<std::size_t>(owner.capacity()) * sizeof(T);
            EXPECT_LE(previous_end, base + owner.allocated_bytes());
        });
    }
    auto const capacity{owner.capacity()};
    owner.reserve(capacity);
    EXPECT_EQ(owner.capacity(), capacity);
    owner.set_num(129);
    owner.remove_at_swap(3, 2);
    EXPECT_EQ(owner.get_view().nested.ys[3], 127.f);
    EXPECT_EQ(owner.num(), 127);
    auto const* data{owner.get_view().bytes.data()};
    auto moved{std::move(owner)};
    EXPECT_EQ(owner.capacity(), 0);
    EXPECT_EQ(moved.get_view().bytes.data(), data);
    owner.reserve(64);
    owner = std::move(moved);
    EXPECT_EQ(owner.get_view().bytes.data(), data);
    EXPECT_EQ(moved.num(), 0);
    EXPECT_EQ(owner.get_view(2, 3).num(), 3);
    owner.reset();
    EXPECT_EQ(owner.num(), 0);
    EXPECT_EQ(owner.capacity(), capacity);
    owner.add_uninitialised(63);
    EXPECT_EQ(owner.get_view().bytes.data(), data);
    owner.get_view().bytes[0] = 42;
    EXPECT_EQ(std::as_const(owner).get_view().bytes[0], 42);
}

TEST(NativeSoa, MatchingSchemaAndMutations) {
    EntityData baseline;
    SingleAllocationEntityData single;
    for (int pass{}; pass < 20; ++pass) {
        baseline.add_defaulted(129);
        single.add_defaulted(129);
        EXPECT_EQ(baseline.num(), single.num());
    }
    auto a{baseline.get_view()};
    auto b{single.get_view()};
    std::size_t columns{};
    std::size_t row_bytes{};
    a.each_column([&](auto column) {
        ++columns;
        row_bytes += sizeof(typename decltype(column)::value_type);
    });
    EXPECT_EQ(columns, 53);
    EXPECT_EQ(row_bytes, 195);
    for (std::int32_t i{}; i < baseline.num(); ++i) {
        a.healths[i] = b.healths[i] = i;
    }
    baseline.remove_at_swap(7, 19);
    single.remove_at_swap(7, 19);
    EXPECT_EQ(baseline.num(), single.num());
    EXPECT_TRUE(std::ranges::equal(baseline.get_view().healths, single.get_view().healths));
    baseline.reset();
    single.reset();
    baseline.set_num(65);
    single.set_num(65);
    EXPECT_TRUE(std::ranges::equal(baseline.get_view().healths, single.get_view().healths));
    EXPECT_EQ(single.get_view().entity_handles[0].index, -1);
}

TEST(NativeSoa, CheckedCapacityArithmetic) {
    using Storage = SingleAllocationEntityData;
    std::int32_t result{};
    EXPECT_FALSE(native_soa::try_round_capacity(-1, Storage::max_capacity, result));
    EXPECT_FALSE(native_soa::try_round_capacity(
        std::int64_t{Storage::max_capacity} + 1, Storage::max_capacity, result));
    EXPECT_TRUE(native_soa::try_round_capacity(65, Storage::max_capacity, result));
    EXPECT_EQ(result, 128);
    EXPECT_EQ(native_soa::allocation_bytes(128, Storage::block_bytes), 2 * Storage::block_bytes);
}
}
