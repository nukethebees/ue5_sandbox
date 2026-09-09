#include <gtest/gtest.h>
#include <native_soa_types.h>

namespace ml::native_soa_tests {
using namespace native_experiment;
template <typename View>
auto array_columns(View view) {
    if constexpr (requires { view.columns(); }) {
        return view.columns();
    } else {
        return view;
    }
}

static_assert(!native_soa::supported_leaf<std::string>);
static_assert(!native_soa::supported_leaf<float const>);
static_assert(!native_soa::supported_leaf<float volatile>);
static_assert(!native_soa::supported_leaf<float&>);
static_assert(!native_soa::supported_leaf<float[3]>);
static_assert(!native_soa::supported_leaf<void>);
static_assert(!std::is_copy_constructible_v<SingleAllocationEntityData>);
static_assert(
    std::is_same_v<decltype(std::declval<SingleAllocationEntityData const&>().get_view().healths()),
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
    EXPECT_EQ(array_columns(owner.get_view()).bytes.data(), nullptr);
    for (std::int32_t const count : {1, 3, 17, 63, 64, 65, 127, 128, 129, 4097, 65537}) {
        auto const previous{owner.num()};
        owner.set_num(count);
        EXPECT_EQ(owner.num(), count);
        EXPECT_GE(owner.capacity(), count);
        EXPECT_EQ(owner.capacity() % 64, 0);
        auto const view{array_columns(owner.get_view())};
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
            auto const cursor{previous_end == 0 ? base : previous_end + 192};
            auto const alignment{std::max(std::size_t{64}, alignof(T))};
            EXPECT_EQ(address - base, ((cursor - base + alignment - 1) / alignment) * alignment);
            previous_end = address + static_cast<std::size_t>(owner.capacity()) * sizeof(T);
            EXPECT_LE(previous_end, base + owner.allocated_bytes());
        });
        EXPECT_EQ(previous_end - base, owner.allocated_bytes());
    }
    auto const capacity{owner.capacity()};
    owner.reserve(capacity);
    EXPECT_EQ(owner.capacity(), capacity);
    owner.set_num(129);
    owner.remove_at_swap(3, 2);
    EXPECT_EQ(array_columns(owner.get_view()).nested.ys[3], 127.f);
    EXPECT_EQ(owner.num(), 127);
    auto const* data{array_columns(owner.get_view()).bytes.data()};
    auto moved{std::move(owner)};
    EXPECT_EQ(owner.capacity(), 0);
    EXPECT_EQ(array_columns(moved.get_view()).bytes.data(), data);
    owner.reserve(64);
    owner = std::move(moved);
    EXPECT_EQ(array_columns(owner.get_view()).bytes.data(), data);
    EXPECT_EQ(moved.num(), 0);
    EXPECT_EQ(array_columns(owner.get_view(2, 3)).num(), 3);
    owner.reset();
    EXPECT_EQ(owner.num(), 0);
    EXPECT_EQ(owner.capacity(), capacity);
    owner.add_uninitialised(63);
    EXPECT_EQ(array_columns(owner.get_view()).bytes.data(), data);
    array_columns(owner.get_view()).bytes[0] = 42;
    EXPECT_EQ(std::as_const(owner).get_view().bytes()[0], 42);
}

TEST(NativeSoa, MatchingSchemaAndMutations) {
    EntityData baseline;
    SingleAllocationEntityData single;
    for (int pass{}; pass < 20; ++pass) {
        baseline.add_defaulted(129);
        single.add_defaulted(129);
        EXPECT_EQ(baseline.num(), single.num());
    }
    auto a{array_columns(baseline.get_view())};
    auto b{array_columns(single.get_view())};
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
    EXPECT_TRUE(std::ranges::equal(array_columns(baseline.get_view()).healths,
                                   array_columns(single.get_view()).healths));
    baseline.reset();
    single.reset();
    baseline.set_num(65);
    single.set_num(65);
    EXPECT_TRUE(std::ranges::equal(array_columns(baseline.get_view()).healths,
                                   array_columns(single.get_view()).healths));
    EXPECT_EQ(array_columns(single.get_view()).entity_handles[0].index, -1);
}

TEST(NativeSoa, CheckedCapacityArithmetic) {
    using Storage = SingleAllocationEntityData;
    std::int32_t result{};
    EXPECT_FALSE(native_soa::try_round_capacity(-1, Storage::max_capacity, result));
    EXPECT_FALSE(native_soa::try_round_capacity(
        std::int64_t{Storage::max_capacity} + 1, Storage::max_capacity, result));
    EXPECT_TRUE(native_soa::try_round_capacity(65, Storage::max_capacity, result));
    EXPECT_EQ(result, 128);
    EXPECT_LE(Storage::layout_bytes(2), 2 * Storage::capacity_block_bound);
}

template <typename Owner>
void check_layout_limits() {
    using namespace single_allocation_layout;
    auto const maximum{Owner::max_capacity};
    EXPECT_EQ(maximum % capacity_granularity, 0);
    std::int32_t rounded{};
    EXPECT_TRUE(try_round_capacity(maximum, maximum, rounded));
    EXPECT_EQ(rounded, maximum);
    EXPECT_FALSE(try_round_capacity(std::int64_t{maximum} + 1, maximum, rounded));
    EXPECT_FALSE(try_round_capacity(std::numeric_limits<std::int64_t>::max(), maximum, rounded));
    for (auto const capacity : {0, 64, 128, maximum - 64, maximum}) {
        std::size_t expected{};
        typename Owner::ConstView{}.columns().each_column([&](auto column) {
            using T = typename decltype(column)::value_type;
            if (expected != 0) {
                expected += 192;
            }
            auto const alignment{std::max(std::size_t{64}, alignof(T))};
            expected = ((expected + alignment - 1) / alignment) * alignment;
            expected += static_cast<std::size_t>(capacity) * sizeof(T);
        });
        auto const blocks{static_cast<std::size_t>(capacity / capacity_granularity)};
        EXPECT_EQ(Owner::layout_bytes(blocks), expected);
        EXPECT_LE(expected, static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max()));
        EXPECT_LE(expected, blocks * Owner::capacity_block_bound);
    }
    Owner owner;
    EXPECT_DEATH(owner.reserve(-1), "invalid");
    EXPECT_DEATH(owner.reserve(maximum + 1), "invalid");
    EXPECT_DEATH(owner.add_uninitialised(maximum + 1), "invalid");
    EXPECT_EQ(owner.capacity(), 0);
}

TEST(NativeSoa, GeneratedLayoutLimits) {
    check_layout_limits<SingleAllocationEntityData>();
    check_layout_limits<SingleAllocationAlignmentData>();
    std::size_t bytes{17};
    EXPECT_FALSE(
        native_soa::try_allocation_bytes(64, std::numeric_limits<std::size_t>::max(), bytes));
    EXPECT_EQ(bytes, 17);
    EXPECT_FALSE(native_soa::try_allocation_bytes(65, 256, bytes));
    EXPECT_DEATH(native_soa::layout_align(std::numeric_limits<std::size_t>::max(), 64), "");
    EXPECT_DEATH(native_soa::layout_align(64, 3), "");
    EXPECT_DEATH(native_soa::layout_align(64, 0), "");
}

TEST(NativeSoa, VectorViewContracts) {
    using View = native_soa::Vector3View<double>;
    using ConstView = native_soa::Vector3ConstView<double>;
    static_assert(sizeof(View) == 16 && sizeof(ConstView) == 16);
    static_assert(std::is_trivially_copyable_v<View> && std::is_trivially_copyable_v<ConstView>);
    static_assert(std::is_convertible_v<View, ConstView>);
    static_assert(!std::is_convertible_v<ConstView, View>);
    static_assert(!std::is_constructible_v<View, std::vector<double>&&>);
    View empty;
    EXPECT_TRUE(empty.is_empty());
    EXPECT_EQ(empty.num(), 0);
    empty.each_column([](auto column) {
        EXPECT_TRUE(column.empty());
        EXPECT_EQ(column.size(), 0);
        EXPECT_EQ(column.data(), nullptr);
        EXPECT_EQ(column.begin(), column.end());
    });
    EXPECT_EQ(empty.slice(0, 0).zs().data(), nullptr);
    double values[96]{};
    View view{values, 256, 4};
    EXPECT_EQ(view.xs().begin() + 4, view.xs().end());
    EXPECT_EQ(view.right(0).zs().data(), values + 68);
    EXPECT_TRUE(view.right(0).zs().empty());
    EXPECT_DEATH((View{nullptr, 256, 1}), "invalid");
    EXPECT_DEATH((View{values, 256, -1}), "invalid");
    EXPECT_DEATH((View{values, 257, 4}), "invalid");
    EXPECT_DEATH((View{values, 24, 4}), "invalid");
    EXPECT_DEATH((View{values, std::size_t{1} << 32, 0}), "invalid");
    EXPECT_DEATH(view.slice(-1, 1), "invalid");
    EXPECT_DEATH(view.slice(0, -1), "invalid");
    EXPECT_DEATH(view.slice(4, 1), "invalid");
    EXPECT_DEATH(view.slice(5, 0), "invalid");
    EXPECT_DEATH(view.left(5), "invalid");
    EXPECT_DEATH(view.right(-1), "invalid");
    EXPECT_DEATH(view.right(5), "invalid");
}

TEST(NativeSoa, CompactViewsAndBulkAppend) {
    static_assert(sizeof(native_soa::Vector2View<double>) == 16);
    static_assert(sizeof(native_soa::Vector2ConstView<float>) == 16);
    static_assert(sizeof(native_soa::Vector3View<float>) == 16);
    static_assert(sizeof(native_soa::Vector3ConstView<double>) == 16);
    double components[96]{};
    native_soa::Vector3View<double> vectors{components, 256, 4};
    vectors.slice(1, 2).ys()[0] = 17.;
    EXPECT_EQ(components[33], 17.);
    EXPECT_EQ(vectors.right(0).zs().data(), components + 68);
    native_soa::Vector3ConstView<double> readonly{vectors.slice(1, 2)};
    EXPECT_EQ(readonly.ys()[0], 17.);
    EXPECT_EQ(readonly.columns().zs.data(), components + 65);
    native_soa::Vector2View<double> xy{components, 256, 4};
    EXPECT_EQ(xy.slice(1, 2).ys()[0], 17.);
    EXPECT_EQ(xy.slice(1, 2).byte_stride(), 256);
    using Owner = SingleAllocationEntityData;
    static_assert(sizeof(Owner::View) == 16 && sizeof(Owner::ConstView) == 16);
    static_assert(std::is_same_v<decltype(std::declval<Owner::View>().view_locations()),
                                 native_soa::Vector3View<float>>);
    Owner source;
    source.set_num(129);
    auto view{source.get_view()};
    for (std::int32_t row{}; row < source.num(); ++row) {
        view.healths()[row] = row;
        view.view_locations().xs()[row] = static_cast<float>(row);
    }
    auto slice{view.slice(1, 64)};
    source.reserve(1024);
    slice = source.slice(1, 64);
    EXPECT_EQ(slice.healths()[63], 64);
    Owner::ConstView const_view{slice};
    EXPECT_EQ(const_view.view_locations().xs()[0], 1.f);
    Owner destination;
    EXPECT_EQ(destination.append_from(source), 0);
    EXPECT_EQ(destination.append_from(const_view), 129);
    EXPECT_EQ(destination.get_view().healths()[192], 64);
    auto const first{destination.num()};
    EXPECT_EQ(destination.append_from(destination), first);
    EXPECT_EQ(destination.num(), 2 * first);
    auto self{destination.slice(63, 65)};
    destination.append_from(self);
    EXPECT_EQ(destination.get_view().healths()[2 * first], 63);
    EXPECT_EQ(destination.append_from(destination.left(0)), destination.num());
    source.get_view().healths()[0] = -1;
    EXPECT_EQ(destination.get_view().healths()[0], 0);
}

TEST(NativeSoa, BulkAppendPreservesEveryAlignedLeaf) {
    SingleAllocationAlignmentData source;
    source.set_num(129);
    source.get_view().each_column([](auto column) {
        for (std::size_t row{}; row < column.size(); ++row) {
            std::memset(&column[row], static_cast<int>(row + 1), sizeof(column[row]));
        }
    });
    SingleAllocationAlignmentData destination;
    destination.append_from(source);
    destination.append_from(destination.slice(1, 128));
    destination.get_const_view().each_column([](auto column) {
        using Element = typename decltype(column)::value_type;
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(column.data()) % alignof(Element), 0u);
        for (std::size_t row{}; row < column.size(); ++row) {
            auto const expected{static_cast<unsigned char>(row < 129 ? row + 1 : row - 127)};
            auto const bytes{std::as_bytes(column.subspan(row, 1))};
            for (auto byte : bytes) {
                EXPECT_EQ(std::to_integer<unsigned char>(byte), expected);
            }
        }
    });
}

TEST(NativeSoa, DescendingRemovalExhaustiveSubsets) {
    for (std::int32_t count{}; count <= 10; ++count) {
        for (unsigned mask{}; mask < (1u << count); ++mask) {
            SingleAllocationEntityData owner;
            owner.set_num(count);
            auto view{owner.get_view()};
            std::vector<std::int32_t> indices;
            for (auto row{count - 1}; row >= 0; --row) {
                view.healths()[row] = row;
                view.view_locations().xs()[row] = static_cast<float>(row);
                if (mask & (1u << row)) {
                    indices.push_back(row);
                }
            }
            auto const final_count{count - static_cast<std::int32_t>(indices.size())};
            std::vector<std::int32_t> tail;
            for (auto row{final_count}; row < count; ++row) {
                if (!(mask & (1u << row))) {
                    tail.push_back(row);
                }
            }
            std::vector<std::int32_t> expected;
            std::size_t next{};
            for (std::int32_t row{}; row < final_count; ++row) {
                expected.push_back(mask & (1u << row) ? tail[next++] : row);
            }
            auto const capacity{owner.capacity()};
            owner.remove_at_swap(std::span<std::int32_t const>{indices});
            EXPECT_EQ(owner.capacity(), capacity);
            EXPECT_TRUE(std::ranges::equal(owner.get_view().healths(), expected));
            for (std::int32_t row{}; row < final_count; ++row) {
                EXPECT_EQ(owner.get_view().view_locations().xs()[row],
                          static_cast<float>(expected[row]));
            }
        }
    }
}

TEST(NativeSoa, InvalidBulkOperationsFailBeforeMutation) {
    SingleAllocationEntityData owner;
    owner.set_num(3);
    std::int32_t const ascending[]{0, 1};
    std::int32_t const duplicate[]{1, 1};
    std::int32_t const negative[]{-1};
    std::int32_t const outside[]{3};
    EXPECT_DEATH(owner.remove_at_swap(std::span<std::int32_t const>{ascending}), "invalid");
    EXPECT_DEATH(owner.remove_at_swap(std::span<std::int32_t const>{duplicate}), "invalid");
    EXPECT_DEATH(owner.remove_at_swap(std::span<std::int32_t const>{negative}), "invalid");
    EXPECT_DEATH(owner.remove_at_swap(std::span<std::int32_t const>{outside}), "invalid");
    auto stale{owner.get_view()};
    owner.reset();
    EXPECT_DEATH(owner.append_from(stale), "invalid");
    EXPECT_EQ(owner.num(), 0);
    owner.set_num(1);
    soa_storage_detail::StorageState huge_state{nullptr,
                                                SingleAllocationEntityData::max_capacity,
                                                SingleAllocationEntityData::max_capacity};
    SingleAllocationEntityData::ConstView huge{
        &huge_state, 0, SingleAllocationEntityData::max_capacity};
    EXPECT_DEATH(owner.append_from(huge), "invalid");
}
}
