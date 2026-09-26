#include <gtest/gtest.h>
#include <native_soa_types.h>

#include <array>
#include <span>

namespace ml::native_soa_tests {
using namespace native_soa_fixture;

static_assert(!native_soa::supported_leaf<std::string>);
static_assert(!native_soa::supported_leaf<float const>);
static_assert(!native_soa::supported_leaf<float volatile>);
static_assert(!native_soa::supported_leaf<float&>);
static_assert(!native_soa::supported_leaf<float[3]>);
static_assert(!native_soa::supported_leaf<void>);
static_assert(!std::is_copy_constructible_v<SingleRows>);
template <typename T>
concept CanReset = requires(T& value) { value.reset(); };
template <typename T>
concept CanAddUninitialised = requires(T& value) { value.add_uninitialised(1); };
template <typename T>
concept CanAssignFirst = requires(T value) { value.assign_first(1.0f); };
static_assert(!CanReset<ApiOwner>);
static_assert(!CanAddUninitialised<ApiOwner>);
static_assert(CanAssignFirst<ApiView>);
static_assert(!CanAssignFirst<ApiConstView>);
static_assert(!CanReset<DualApiRows>);
static_assert(!CanReset<DualCompactRows>);
static_assert(soa_storage_detail::validate_compact_view<ApiView>());
static_assert(soa_storage_detail::validate_compact_view<ApiConstView>());
static_assert(std::is_same_v<ApiOwner::Value, ApiConstView::Value>);
static_assert(std::is_same_v<decltype(ApiReference::owner), ApiOwner*>);
static_assert(std::is_same_v<decltype(std::declval<SingleRows const&>().get_view().values()),
                             std::span<std::int32_t const>>);

TEST(NativeSoa, MutuallyReferencingPointersCompileInBothOrdersAndAcrossDeclarationKinds) {
    PointerA first;
    PointerB second;
    first.add(&second);
    second.add(&first);
    EXPECT_EQ(first.get_view().others[0], &second);
    EXPECT_EQ(second.get_view().others[0], &first);
    ReverseB reverse_second;
    ReverseA reverse_first;
    reverse_second.add(&reverse_first);
    reverse_first.add(&reverse_second);
    EXPECT_EQ(reverse_second.get_view().others[0], &reverse_first);
    EXPECT_EQ(reverse_first.get_view().others[0], &reverse_second);
    MixedRows rows;
    MixedRecord record{&rows};
    rows.add(&record);
    EXPECT_EQ(record.rows->get_view().records[0], &record);
}

TEST(NativeSoa, LogicalApiSurvivesCompactOnlyStorageAndNestedLayouts) {
    ApiOwner owner;
    owner.set_num(2);
    auto view{owner.get_view()};
    view.assign_first(7.0f);
    view.values()[1] = 9.0f;
    auto positions{view.view_positions()};
    positions.xs()[0] = 2.0f;
    positions.ys()[0] = 3.0f;
    positions.shift_x(4.0f);
    view.masks()[0].set(ApiField::Values);

    EXPECT_EQ(owner.first_value(), 7.0f);
    EXPECT_EQ(owner.total(), 16.0f);
    EXPECT_EQ(view.first_x(), 6.0f);
    EXPECT_EQ(owner.get_const_view().first_x(), 6.0f);
    EXPECT_EQ(positions.x_at(0), 6.0f);
    EXPECT_EQ(positions[0].y, 3.0f);
    EXPECT_TRUE(view.masks()[0].has(ApiField::Values));

    owner.reserve(owner.capacity() + 1);
    EXPECT_EQ(view.first_x(), 6.0f);
    EXPECT_EQ(positions.x_at(0), 6.0f);
    EXPECT_EQ(owner.append_from(view.slice(0, 1)), 2);
    owner.copy_elements(1, owner.get_const_view(), 0, 2);
    EXPECT_EQ(owner.get_const_view().values()[2], 9.0f);
    EXPECT_EQ(owner.get_const_view().view_positions()[1].x, 6.0f);

    EquivalentOwner equivalent;
    equivalent.set_num(1);
    equivalent.get_view().xs()[0] = 5.0f;
    equivalent.get_view().ys()[0] = 8.0f;
    EXPECT_EQ(equivalent[0].x, 5.0f);
    EXPECT_EQ(equivalent.get_const_view()[0].y, 8.0f);
}

TEST(NativeSoa, BothStorageImplementationsShareLogicalFunctionsAndOperationSelection) {
    auto verify = []<typename Owner>() {
        Owner owner;
        owner.set_num(1);
        owner.get_view().assign(4.0f);
        EXPECT_EQ(owner.row_count(), 1);
        EXPECT_EQ(owner.get_const_view().first(), 4.0f);
    };
    verify.operator()<DualApiRows>();
    verify.operator()<DualCompactRows>();
}

struct ApiSource {
    struct Positions {
        std::array<float, 2> x{2.0f, 4.0f};
        std::array<float, 2> y{3.0f, 5.0f};
        auto xs() const -> std::span<float const> { return x; }
        auto ys() const -> std::span<float const> { return y; }
    } positions;
    std::array<float, 2> value_columns{11.0f, 13.0f};
    std::array<ApiMask, 2> mask_columns{};
    auto num() const -> std::int32_t { return 2; }
    void validate() const {}
    auto values() const -> std::span<float const> { return value_columns; }
    auto masks() const -> std::span<ApiMask const> { return mask_columns; }
    auto view_positions() const -> Positions const& { return positions; }
};

TEST(NativeSoa, LogicalApiAcceptsIndependentColumnsAndCompactSourcesDuringGrowth) {
    ApiSource source;
    source.mask_columns[1].set(ApiField::Values);
    ApiOwner owner;
    owner.append_from(source);
    EXPECT_EQ(owner.total(), 24.0f);
    owner.set_num(owner.capacity());
    auto const compact{owner.get_const_view().left(2)};
    auto const first{owner.append_from(compact)};
    EXPECT_EQ(owner.get_const_view().values()[first + 1], 13.0f);
    EXPECT_EQ(owner.get_const_view().view_positions()[first + 1].y, 5.0f);
    EXPECT_TRUE(owner.get_const_view().masks()[first + 1].has(ApiField::Values));
    EXPECT_EQ(compact.first_x(), 2.0f);
}

TEST(NativeSoa, CompactSelfCopyPreservesEveryColumn) {
    struct CopyRange {
        char const* name;
        std::int32_t source;
        std::int32_t destination;
        std::int32_t count;
        bool single_element{};
    };
    constexpr CopyRange cases[]{
        {"source before destination", 0, 2, 5},
        {"destination before source", 2, 0, 5},
        {"identical ranges", 1, 1, 5},
        {"disjoint forward", 0, 5, 3},
        {"disjoint backward", 5, 0, 3},
        {"one element", 3, 6, 1, true},
        {"identical element", 3, 3, 1, true},
        {"empty range", 2, 4, 0},
    };
    constexpr std::int32_t row_count{8};
    for (auto const& range : cases) {
        SCOPED_TRACE(range.name);
        for (bool const sliced : {false, true}) {
            SCOPED_TRACE(sliced);
            SingleRows owner;
            owner.set_num(row_count);
            auto const view{owner.get_view()};
            std::int32_t column_index{};
            view.each_column([&](auto column) {
                using Element = std::remove_cvref_t<decltype(column[0])>;
                for (std::int32_t row{}; row < row_count; ++row) {
                    column[row] = static_cast<Element>(column_index * 16 + row);
                }
                ++column_index;
            });
            auto const capacity{owner.capacity()};
            auto const source_first{sliced ? range.source : 0};
            auto const source{owner.get_const_view(source_first, row_count - source_first)};
            if (range.single_element) {
                owner.copy_element(range.destination, source, range.source - source_first);
            } else {
                owner.copy_elements(
                    range.destination, source, range.source - source_first, range.count);
            }

            EXPECT_EQ(owner.num(), row_count);
            EXPECT_EQ(owner.capacity(), capacity);
            column_index = 0;
            view.each_column([&](auto const column) {
                using Element = std::remove_cvref_t<decltype(column[0])>;
                SCOPED_TRACE(column_index);
                for (std::int32_t row{}; row < row_count; ++row) {
                    auto const copied{row >= range.destination &&
                                      row < range.destination + range.count};
                    auto const original{copied ? range.source + row - range.destination : row};
                    EXPECT_EQ(column[row], static_cast<Element>(column_index * 16 + original));
                }
                ++column_index;
            });
            EXPECT_EQ(column_index, 7);
        }
    }
}

TEST(NativeSoa, LayoutGrowthAndMoves) {
    AlignmentRows vectors;
    vectors.set_num(129);
    vectors.each_column([](auto const& column) {
        using Element = typename std::remove_cvref_t<decltype(column)>::value_type;
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(column.data()) % alignof(Element), 0u);
#if NATIVE_SOA_MIMALLOC
        EXPECT_TRUE(sbx::memory::owns(column.data()));
#endif
    });
    SingleAlignmentRows owner;
    EXPECT_EQ(owner.num(), 0);
    EXPECT_EQ(owner.capacity(), 0);
    EXPECT_EQ(owner.get_view().bytes().data(), nullptr);
    for (std::int32_t const count : {1, 3, 17, 63, 64, 65, 127, 128, 129, 4097, 65537}) {
        auto const previous{owner.num()};
        owner.set_num(count);
        EXPECT_EQ(owner.num(), count);
        EXPECT_GE(owner.capacity(), count);
        EXPECT_EQ(owner.capacity() % 64, 0);
        auto const view{owner.get_view()};
#if NATIVE_SOA_MIMALLOC
        EXPECT_TRUE(sbx::memory::owns(view.bytes().data()));
#endif
        EXPECT_EQ(view.aligned32().back().value, 32);
        EXPECT_EQ(view.aligned64().back().value, 64);
        EXPECT_EQ(view.aligned256().back().value, 256);
        EXPECT_EQ(view.view_nested().xs().back(), 0.f);
        for (std::int32_t i{}; i < previous; ++i) {
            EXPECT_EQ(view.view_nested().ys()[i], static_cast<float>(i));
        }
        for (std::int32_t i{}; i < count; ++i) {
            view.view_nested().ys()[i] = static_cast<float>(i);
        }
        std::uintptr_t previous_end{};
        auto const base{reinterpret_cast<std::uintptr_t>(view.bytes().data())};
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
    EXPECT_EQ(owner.get_view().view_nested().ys()[3], 127.f);
    EXPECT_EQ(owner.num(), 127);
    auto const* data{owner.get_view().bytes().data()};
    auto moved{std::move(owner)};
    // NOLINTNEXTLINE(bugprone-use-after-move): Verify the defined moved-from state.
    EXPECT_EQ(owner.capacity(), 0);
    EXPECT_EQ(moved.get_view().bytes().data(), data);
    owner.reserve(64);
    owner = std::move(moved);
    EXPECT_EQ(owner.get_view().bytes().data(), data);
    // NOLINTNEXTLINE(bugprone-use-after-move): Verify the defined moved-from state.
    EXPECT_EQ(moved.num(), 0);
    EXPECT_EQ(owner.get_view(2, 3).num(), 3);
    owner.reset();
    EXPECT_EQ(owner.num(), 0);
    EXPECT_EQ(owner.capacity(), capacity);
    owner.add_uninitialised(63);
    EXPECT_EQ(owner.get_view().bytes().data(), data);
    owner.get_view().bytes()[0] = 42;
    EXPECT_EQ(std::as_const(owner).get_view().bytes()[0], 42);
}

TEST(NativeSoa, MatchingSchemaAndMutations) {
    Rows baseline;
    SingleRows single;
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
    std::size_t single_columns{};
    std::size_t single_row_bytes{};
    b.each_column([&](auto column) {
        ++single_columns;
        single_row_bytes += sizeof(typename decltype(column)::value_type);
    });
    EXPECT_EQ(columns, single_columns);
    EXPECT_EQ(row_bytes, single_row_bytes);
    for (std::int32_t i{}; i < baseline.num(); ++i) {
        a.values[i] = b.values()[i] = i;
    }
    baseline.remove_at_swap(7, 19);
    single.remove_at_swap(7, 19);
    EXPECT_EQ(baseline.num(), single.num());
    EXPECT_TRUE(std::ranges::equal(baseline.get_view().values, single.get_view().values()));
    baseline.reset();
    single.reset();
    baseline.set_num(65);
    single.set_num(65);
    EXPECT_TRUE(std::ranges::equal(baseline.get_view().values, single.get_view().values()));
    EXPECT_EQ(single.get_view().values()[0], 0);
}

TEST(NativeSoa, VectorSwapRemovalHandlesEveryBoundaryCase) {
    auto const check = [](std::int32_t const index,
                          std::int32_t const count,
                          std::span<std::int32_t const> const expected) {
        Rows owner;
        owner.set_num(6);
        auto columns{owner.get_view()};
        for (std::int32_t row{}; row < owner.num(); ++row) {
            columns.values[row] = row;
            columns.positions.xs()[row] = static_cast<float>(row);
        }

        owner.remove_at_swap(index, count);

        ASSERT_EQ(owner.num(), static_cast<std::int32_t>(expected.size()));
        columns = owner.get_view();
        std::int32_t expected_index{};
        for (auto const expected_value : expected) {
            EXPECT_EQ(columns.values[expected_index], expected_value);
            EXPECT_EQ(columns.positions.xs()[expected_index], static_cast<float>(expected_value));
            ++expected_index;
        }
    };
    std::array const unchanged{0, 1, 2, 3, 4, 5};
    std::array const one_removed{0, 1, 5, 3, 4};
    std::array const beginning_removed{4, 5, 2, 3};
    std::array const end_removed{0, 1, 2, 3, 4};
    std::array const trailing_removed{0, 1, 5};
    std::array<std::int32_t, 0> const all_removed;

    check(0, 0, unchanged);
    check(2, 1, one_removed);
    check(0, 2, beginning_removed);
    check(5, 1, end_removed);
    check(2, 3, trailing_removed);
    check(0, 6, all_removed);
}

TEST(NativeSoa, CheckedCapacityArithmetic) {
    using Storage = SingleRows;
    std::int32_t result{};
    EXPECT_FALSE(native_soa::try_round_capacity(-1, Storage::max_capacity, result));
    EXPECT_FALSE(native_soa::try_round_capacity(
        std::int64_t{Storage::max_capacity} + 1, Storage::max_capacity, result));
    EXPECT_TRUE(native_soa::try_round_capacity(65, Storage::max_capacity, result));
    EXPECT_EQ(result, 128);
    EXPECT_LE(Storage::layout_bytes(2), 2 * Storage::capacity_block_bound);
}

TEST(NativeSoa, ColumnOffsetsMatchSequentialLayoutAcrossAlignments) {
    struct alignas(256) Overaligned {
        std::byte bytes[256];
    };
    single_allocation_layout::ColumnLayoutStart const start{};
    single_allocation_layout::ColumnLayout<std::int32_t> const first{start};
    // Construction chains a new column after first.
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
    single_allocation_layout::ColumnLayout<std::int32_t> const second{first};
    single_allocation_layout::ColumnLayout<Overaligned> const third{second};
    single_allocation_layout::ColumnLayout<std::byte> const fourth{third};
    single_allocation_layout::ColumnLayout<Overaligned> const fifth{fourth};
    single_allocation_layout::ColumnLayout<float> const sixth{fifth};

    for (std::size_t blocks{}; blocks < 1025; ++blocks) {
        single_allocation_layout::LayoutCursor cursor{blocks};
        EXPECT_EQ(cursor.advance(first), first.offset(blocks));
        EXPECT_EQ(cursor.advance(second), second.offset(blocks));
        EXPECT_EQ(cursor.advance(third), third.offset(blocks));
        EXPECT_EQ(cursor.advance(fourth), fourth.offset(blocks));
        EXPECT_EQ(cursor.advance(fifth), fifth.offset(blocks));
        EXPECT_EQ(cursor.advance(sixth), sixth.offset(blocks));
        EXPECT_EQ(third.data_end(blocks), third.offset(blocks) + blocks * 64 * sizeof(Overaligned));
    }
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
        typename Owner::ConstView{}.each_column([&](auto column) {
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
    check_layout_limits<SingleRows>();
    check_layout_limits<SingleAlignmentRows>();
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
    static_assert(soa_storage_detail::validate_compact_view<View>());
    static_assert(soa_storage_detail::validate_compact_view<ConstView>());
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
    static_assert(soa_storage_detail::validate_compact_view<native_soa::Vector2View<double>>());
    static_assert(soa_storage_detail::validate_compact_view<native_soa::Vector2ConstView<float>>());
    static_assert(soa_storage_detail::validate_compact_view<native_soa::Vector3View<float>>());
    static_assert(
        soa_storage_detail::validate_compact_view<native_soa::Vector3ConstView<double>>());
    double components[96]{};
    native_soa::Vector3View<double> vectors{components, 256, 4};
    vectors.slice(1, 2).ys()[0] = 17.;
    EXPECT_EQ(components[33], 17.);
    EXPECT_EQ(vectors.right(0).zs().data(), components + 68);
    native_soa::Vector3ConstView<double> readonly{vectors.slice(1, 2)};
    EXPECT_EQ(readonly.ys()[0], 17.);
    EXPECT_EQ(readonly.zs().data(), components + 65);
    native_soa::Vector2View<double> xy{components, 256, 4};
    EXPECT_EQ(xy.slice(1, 2).ys()[0], 17.);
    EXPECT_EQ(xy.slice(1, 2).byte_stride(), 256);
    using Owner = SingleRows;
    static_assert(soa_storage_detail::validate_compact_view<Owner::View>());
    static_assert(soa_storage_detail::validate_compact_view<Owner::ConstView>());
    static_assert(std::is_same_v<decltype(std::declval<Owner::View>().view_positions()),
                                 native_soa::Vector3View<float>>);
    Owner source;
    source.set_num(129);
    auto view{source.get_view()};
    for (std::int32_t row{}; row < source.num(); ++row) {
        view.values()[row] = row;
        view.view_positions().xs()[row] = static_cast<float>(row);
    }
    auto slice{view.slice(1, 64)};
    source.reserve(1024);
    EXPECT_EQ(slice.values()[63], 64);
    EXPECT_EQ(view.view_positions().xs()[1], 1.f);
    Owner::ConstView const_view{slice};
    EXPECT_EQ(const_view.view_positions().xs()[0], 1.f);
    Owner destination;
    EXPECT_EQ(destination.append_from(source.get_const_view()), 0);
    EXPECT_EQ(destination.append_from(const_view), 129);
    EXPECT_EQ(destination.get_view().values()[192], 64);
    auto const first{destination.num()};
    EXPECT_EQ(destination.append_from(destination.get_const_view()), first);
    EXPECT_EQ(destination.num(), 2 * first);
    auto self{destination.slice(63, 65)};
    destination.append_from(self);
    EXPECT_EQ(destination.get_view().values()[2 * static_cast<std::size_t>(first)], 63);
    EXPECT_EQ(destination.append_from(destination.left(0)), destination.num());
    source.get_view().values()[0] = -1;
    EXPECT_EQ(destination.get_view().values()[0], 0);
}

TEST(NativeSoa, BulkAppendPreservesEveryAlignedLeaf) {
    SingleAlignmentRows source;
    source.set_num(129);
    source.get_view().each_column([](auto column) {
        for (std::size_t row{}; row < column.size(); ++row) {
            std::memset(&column[row], static_cast<int>(row + 1), sizeof(column[row]));
        }
    });
    SingleAlignmentRows destination;
    destination.append_from(source.get_const_view());
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

struct IndependentRows {
    std::array<std::int32_t, 65> values_{};
    std::array<float, 65> xs_{}, ys_{}, zs_{};
    auto num() const -> std::int32_t { return 65; }
    void validate() const {}
    auto values() const { return std::span{values_}; }
    auto xs() const { return std::span{xs_}; }
    auto ys() const { return std::span{ys_}; }
    auto zs() const { return std::span{zs_}; }
    auto view_positions() const -> IndependentRows const& { return *this; }
    auto view_velocities() const -> IndependentRows const& { return *this; }
};

TEST(NativeSoa, IndependentColumnsAppendDirectly) {
    IndependentRows source;
    for (std::int32_t row{}; row < source.num(); ++row) {
        source.values_[row] = row + 100;
        source.xs_[row] = static_cast<float>(row);
    }
    SingleRows destination;
    destination.set_num(1);
    destination.get_view().values()[0] = -1;
    EXPECT_EQ(destination.append_from(source), 1);
    EXPECT_EQ(destination.num(), 66);
    EXPECT_EQ(destination.get_const_view().values()[0], -1);
    EXPECT_EQ(destination.get_const_view().values()[65], 164);
    EXPECT_EQ(destination.get_const_view().view_positions().xs()[65], 64.f);
    EXPECT_EQ(source.values_[64], 164);
    destination.reset();
    EXPECT_EQ(destination.append_from(source, 1, 16), 0);
    EXPECT_EQ(destination.append_from(source, 0, 0), 16);
    EXPECT_EQ(destination.append_from(source, 17, 16), 16);
    EXPECT_EQ(destination.num(), 32);
    EXPECT_EQ(destination.get_const_view().values()[0], 101);
    EXPECT_EQ(destination.get_const_view().values()[31], 132);
    EXPECT_DEATH(destination.append_from(source, -1, 1), "invalid");
    EXPECT_DEATH(destination.append_from(source, 64, 2), "invalid");
}

TEST(NativeSoa, DescendingRemovalExhaustiveSubsets) {
    for (std::int32_t count{}; count <= 10; ++count) {
        for (unsigned mask{}; mask < (1u << count); ++mask) {
            SingleRows owner;
            owner.set_num(count);
            auto view{owner.get_view()};
            std::vector<std::int32_t> indices;
            for (auto row{count - 1}; row >= 0; --row) {
                view.values()[row] = row;
                view.view_positions().xs()[row] = static_cast<float>(row);
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
            EXPECT_TRUE(std::ranges::equal(owner.get_view().values(), expected));
            for (std::int32_t row{}; row < final_count; ++row) {
                EXPECT_EQ(owner.get_view().view_positions().xs()[row],
                          static_cast<float>(expected[row]));
            }
        }
    }
}

TEST(NativeSoa, InvalidBulkOperationsFailBeforeMutation) {
    SingleRows owner;
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
    soa_storage_detail::StorageState huge_state{
        nullptr, SingleRows::max_capacity, SingleRows::max_capacity};
    SingleRows::ConstView huge{&huge_state, 0, SingleRows::max_capacity};
    EXPECT_DEATH(owner.append_from(huge), "invalid");
}
}
