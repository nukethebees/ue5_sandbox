#include <SandboxCore/array_utils.h>
#include <SbxCoreExperiments/soa_test_support.h>
#include <SbxCoreExperiments/soa_types.h>

#include "TestHarness.h"

#include <array>
#include <limits>
#include <type_traits>

namespace ml::single_allocation_tests {
using namespace single_allocation_experiment;
using namespace soa_storage;

struct NonTrivialCopy {
    NonTrivialCopy() noexcept = default;
    NonTrivialCopy(NonTrivialCopy const&) {}
};
struct NonTrivialDestructor {
    ~NonTrivialDestructor() {}
};
struct ThrowingDefault {
    ThrowingDefault() noexcept(false) {}
};

static_assert(!supported_leaf<NonTrivialCopy>);
static_assert(!supported_leaf<NonTrivialDestructor>);
static_assert(!supported_leaf<ThrowingDefault>);
static_assert(!supported_leaf<FString>);
static_assert(supported_leaf<Handle>);
static_assert(!std::is_trivially_default_constructible_v<Handle>);
static_assert(!std::is_copy_constructible_v<SingleAllocationEntityData>);
static_assert(!std::is_copy_assignable_v<SingleAllocationEntityData>);
static_assert(std::is_nothrow_move_constructible_v<SingleAllocationEntityData>);
static_assert(std::is_nothrow_move_assignable_v<SingleAllocationEntityData>);
static_assert(sizeof(SingleAllocationEntityData::View) == 16);
static_assert(sizeof(SingleAllocationEntityData::ConstView) == 16);
static_assert(sizeof(SingleAllocationEntityData) == sizeof(void*) + 2 * sizeof(int32));

TEST_CASE("SandboxCore.SingleAllocation.Empty and end views preserve column pointers") {
    SingleAllocationAlignmentData rows;
    auto verify_views = [&](int32 const count) {
        auto full{array_columns(rows.get_view())};
        auto const empty{array_columns(rows.get_view(count, 0))};
        auto const& const_rows{rows};
        auto const const_full{array_columns(const_rows.get_view())};
        auto const const_empty{array_columns(const_rows.get_view(count, 0))};
        static_assert(std::is_same_v<decltype(full.nested.xs.GetData()), float*>);
        static_assert(std::is_same_v<decltype(const_full.nested.xs.GetData()), float const*>);
        CHECK(full.num() == count);
        CHECK(empty.num() == 0);
        CHECK(const_full.num() == count);
        CHECK(const_empty.num() == 0);
        CHECK(full.nested.xs.GetData() == const_full.nested.xs.GetData());
        CHECK(rows.get_view().view_nested().xs().GetData() == full.nested.xs.GetData());
        CHECK(rows.get_view(count, 0).view_nested().xs().GetData() == empty.nested.xs.GetData());
        CHECK(const_rows.get_view().view_nested().xs().GetData() == const_full.nested.xs.GetData());
        CHECK(empty.nested.xs.GetData() == const_empty.nested.xs.GetData());
        if (rows.capacity() == 0) {
            each_leaf(empty, [](auto column) { CHECK(column.GetData() == nullptr); });
            each_leaf(const_full, [](auto column) { CHECK(column.GetData() == nullptr); });
        } else {
            CHECK(empty.nested.xs.GetData() == full.nested.xs.GetData() + count);
            CHECK(empty.aligned256.GetData() == full.aligned256.GetData() + count);
            each_leaf(full, [](auto column) { CHECK(column.GetData() != nullptr); });
        }
    };
    verify_views(0);
    rows.reserve(64);
    verify_views(0);
    rows.add_defaulted(64);
    verify_views(64);
    SingleAllocationAlignmentData moved{std::move(rows)};
    verify_views(0);
    rows.add_defaulted(1);
    verify_views(1);
    rows.reset();
    verify_views(0);
}

TEST_CASE("SandboxCore.SingleAllocation.Empty boundaries and alignment") {
    SingleAllocationAlignmentData rows;
    CHECK(rows.num() == 0);
    CHECK(rows.capacity() == 0);
    CHECK(rows.allocated_bytes() == 0);
    each_leaf(array_columns(rows.get_view()), [](auto column) {
        CHECK(column.Num() == 0);
        CHECK(column.GetData() == nullptr);
    });
    rows.reserve(0);
    rows.add_uninitialised(0);
    rows.add_defaulted(0);
    rows.set_num(0);
    rows.remove_at_swap(0, 0);
    CHECK(rows.capacity() == 0);

    for (int32 const count : {1, 3, 17, 63, 64, 65, 127, 128, 129}) {
        CAPTURE(count);
        SingleAllocationAlignmentData values;
        values.reserve(count);
        CHECK(values.capacity() == ((count + 63) / 64) * 64);
        CHECK(values.num() == 0);
        values.set_num(count);
        auto view{array_columns(values.get_view())};
        auto const base{reinterpret_cast<UPTRINT>(view.bytes.GetData())};
        CHECK(base % values.allocation_alignment == 0);
        UPTRINT end{base};
        each_leaf(view, [&](auto column) {
            using Element = std::remove_cvref_t<decltype(column[0])>;
            auto const address{reinterpret_cast<UPTRINT>(column.GetData())};
            CHECK(column.Num() == count);
            CHECK(address % alignof(Element) == 0);
            CHECK(address % 64 == 0);
            auto const cursor{end + (end == base ? 0 : 192)};
            auto const alignment{std::max(SIZE_T{64}, SIZE_T{alignof(Element)})};
            CHECK(address - base == ((cursor - base + alignment - 1) / alignment) * alignment);
            end = address + static_cast<SIZE_T>(values.capacity()) * sizeof(Element);
            CHECK(end <= base + values.allocated_bytes());
        });
        auto const blocks{static_cast<SIZE_T>(values.capacity() / 64)};
        CHECK(reinterpret_cast<UPTRINT>(view.aligned32.GetData()) - base == values.aligned32_offset(blocks));
        CHECK(reinterpret_cast<UPTRINT>(view.nested.xs.GetData()) - base == values.nested_xs_offset(blocks));
        CHECK(reinterpret_cast<UPTRINT>(view.aligned64.GetData()) - base == values.aligned64_offset(blocks));
        CHECK(reinterpret_cast<UPTRINT>(view.aligned256.GetData()) - base == values.aligned256_offset(blocks));
        CHECK(values.allocated_bytes() == values.layout_bytes(blocks));
        CHECK(view.aligned32[0].value == 32);
        CHECK(view.aligned64[0].value == 64);
        CHECK(view.aligned256[0].value == 256);
        CHECK(view.handles[0].index == -1);
        CHECK(view.handles[0].generation == -1);

        int32 column_index{};
        each_leaf(view, [&](auto column) {
            using Element = std::remove_cvref_t<decltype(column[0])>;
            FMemory::Memset(column.GetData(), ++column_index, static_cast<SIZE_T>(count) * sizeof(Element));
        });
        values.reserve(values.capacity() + 1);
        view = array_columns(values.get_view());
        CHECK(reinterpret_cast<UPTRINT>(view.aligned256.GetData()) % 256 == 0);
        column_index = 0;
        each_leaf(view, [&](auto column) {
            using Element = std::remove_cvref_t<decltype(column[0])>;
            auto const expected{static_cast<uint8>(++column_index)};
            auto const* bytes{reinterpret_cast<uint8 const*>(column.GetData())};
            auto const byte_count{static_cast<SIZE_T>(count) * sizeof(Element)};
            for (SIZE_T index{}; index < byte_count; ++index) {
                REQUIRE(bytes[index] == expected);
            }
        });
    }
}

void check_row_patterns(SingleAllocationEntityData const& values) {
    int32 columns{};
    each_leaf(array_columns(values.get_const_view()), [&](auto column) {
        using Element = std::remove_cvref_t<decltype(column[0])>;
        ++columns;
        auto const count{column.Num()};
        for (int32 row{}; row < count; ++row) {
            std::array<uint8, sizeof(Element)> expected;
            expected.fill(static_cast<uint8>((row + columns) % 127 + 1));
            REQUIRE(FMemory::Memcmp(&column[row], expected.data(), sizeof(Element)) == 0);
        }
    });
    CHECK(columns == 53);
}

TEST_CASE("SandboxCore.SingleAllocation.Every leaf survives repeated coordinated growth") {
    SingleAllocationEntityData values;
    int32 growths{};
    for (int32 row{}; row < 20000; ++row) {
        auto const old_capacity{values.capacity()};
        auto* const old_pointer{array_columns(values.get_view()).entity_handles.GetData()};
        values.add_uninitialised(1);
        CHECK(values.capacity() % 64 == 0);
        if (old_capacity == values.capacity()) {
            REQUIRE(array_columns(values.get_view()).entity_handles.GetData() == old_pointer);
        } else {
            ++growths;
        }
        int32 columns{};
        each_leaf(array_columns(values.get_view()), [&](auto column) {
            using Element = std::remove_cvref_t<decltype(column[0])>;
            FMemory::Memset(&column[row], (row + ++columns) % 127 + 1, sizeof(Element));
        });
        if (old_capacity != values.capacity()) {
            check_row_patterns(values);
        }
    }
    CHECK(growths > 5);
    check_row_patterns(values);
    values.reserve(values.capacity() + 1);
    check_row_patterns(values);
    auto const capacity{values.capacity()};
    values.reserve(1);
    CHECK(values.capacity() == capacity);
}

TEST_CASE("SandboxCore.SingleAllocation.Default resize reset and shared views") {
    SingleAllocationEntityData values;
    values.reserve(128);
    values.add_defaulted(65);
    auto view{array_columns(values.get_view())};
    for (int32 index{}; index < values.num(); ++index) {
        CHECK(view.healths[index] == 0);
        CHECK(view.entity_handles[index].index == -1);
        CHECK(view.locations.xs[index] == 0.f);
    }
    view.locations.xs[2] = 12.f;
    view.healths[2] = 42;
    array_columns(values.get_view(2, 3)).healths[1] = 43;
    SingleAllocationEntityData const& const_values{values};
    static_assert(std::is_same_v<decltype(array_columns(const_values.get_view()).healths[0]), int32 const&>);
    CHECK(array_columns(const_values.get_view()).locations.xs[2] == 12.f);
    CHECK(array_columns(const_values.get_const_view(2, 3)).healths[1] == 43);
    CHECK(values.slice(2, 1).healths()[0] == 42);
    CHECK(values.left(4).healths()[3] == 43);
    CHECK(values.right(63).healths()[0] == 42);
    CHECK(values.get_view(values.num(), 0).num() == 0);

    auto* const pointer{view.healths.GetData()};
    values.set_num(3, EAllowShrinking::Yes);
    CHECK(values.capacity() == 128);
    values.set_num(65);
    CHECK(array_columns(values.get_view()).healths.GetData() == pointer);
    CHECK(array_columns(values.get_view()).healths[2] == 42);
    CHECK(array_columns(values.get_view()).healths[3] == 0);
    values.set_num(129);
    CHECK(array_columns(values.get_view()).healths[2] == 42);
    CHECK(array_columns(values.get_view()).entity_handles[128].index == -1);
    auto const capacity{values.capacity()};
    values.reset();
    CHECK(values.is_empty());
    CHECK(values.capacity() == capacity);
    values.add_defaulted(1);
    CHECK(array_columns(values.get_view()).healths[0] == 0);
    CHECK(array_columns(values.get_view()).entity_handles[0].index == -1);
    ml::fill(array_columns(values.get_view()).healths, 123);
    CHECK(array_columns(values.get_const_view()).healths[0] == 123);
}

TEST_CASE("SandboxCore.SingleAllocation.Swap removal matches generated TArray owner") {
    for (int32 const index : {0, 2, 7, 10}) {
        for (int32 const count : {0, 1, 3, 8, 10}) {
            if (count > 10 - index) {
                continue;
            }
            CAPTURE(index, count);
            SingleAllocationEntityData values;
            EntityData baseline;
            values.add_defaulted(10);
            baseline.add_defaulted(10);
            auto fill = [](auto view) {
                each_leaf(view, [](auto column) {
                    using Element = std::remove_cvref_t<decltype(column[0])>;
                    for (int32 row{}; row < 10; ++row) {
                        FMemory::Memset(&column[row], row + 1, sizeof(Element));
                    }
                });
            };
            fill(array_columns(values.get_view()));
            fill(array_columns(baseline.get_view()));
            values.remove_at_swap(index, count, EAllowShrinking::Yes);
            baseline.remove_at_swap(index, count, EAllowShrinking::No);
            CHECK(values.capacity() == 64);
            CHECK(values.num() == baseline.num());
            std::array<void const*, 53> expected{};
            int32 leaf{};
            each_leaf(array_columns(baseline.get_const_view()), [&](auto column) { expected[leaf++] = column.GetData(); });
            leaf = 0;
            each_leaf(array_columns(values.get_const_view()), [&](auto column) {
                using Element = std::remove_cvref_t<decltype(column[0])>;
                if (values.num() > 0) {
                    CHECK(FMemory::Memcmp(column.GetData(), expected[leaf], static_cast<SIZE_T>(values.num()) * sizeof(Element)) == 0);
                }
                ++leaf;
            });
        }
    }
}

TEST_CASE("SandboxCore.SingleAllocation.Moves transfer ownership and sources remain reusable") {
    SingleAllocationAlignmentData first;
    first.add_defaulted(65);
    array_columns(first.get_view()).nested.ys[64] = 19.f;
    auto* const pointer{array_columns(first.get_view()).bytes.GetData()};
    auto const capacity{first.capacity()};
    SingleAllocationAlignmentData second{std::move(first)};
    CHECK(first.num() == 0);
    CHECK(first.capacity() == 0);
    CHECK(array_columns(first.get_view()).bytes.GetData() == nullptr);
    CHECK(array_columns(second.get_view()).bytes.GetData() == pointer);
    first.add_defaulted(1);
    first = std::move(second);
    CHECK(first.capacity() == capacity);
    CHECK(array_columns(first.get_view()).bytes.GetData() == pointer);
    CHECK(array_columns(first.get_view()).nested.ys[64] == 19.f);
    CHECK(second.capacity() == 0);
    auto* const self{&first};
    first = std::move(*self);
    CHECK(first.num() == 65);
    second.add_defaulted(1);
    CHECK(array_columns(second.get_view()).aligned256[0].value == 256);
}

TEST_CASE("SandboxCore.SingleAllocation.Arithmetic rejects overflow before allocation") {
    int32 rounded{-1};
    auto const maximum{SingleAllocationEntityData::max_capacity};
    CHECK_FALSE(try_round_capacity(-1, maximum, rounded));
    CHECK_FALSE(try_round_capacity(std::numeric_limits<int64>::max(), maximum, rounded));
    CHECK_FALSE(try_round_capacity(static_cast<int64>(maximum) + 1, maximum, rounded));
    CHECK(try_round_capacity(maximum, maximum, rounded));
    CHECK(rounded == maximum);
    CHECK(try_round_capacity(65, maximum, rounded));
    CHECK(rounded == 128);
    SIZE_T bytes{};
    CHECK_FALSE(try_allocation_bytes(-64, 256, bytes));
    CHECK_FALSE(try_allocation_bytes(65, 256, bytes));
    CHECK_FALSE(try_allocation_bytes(64, std::numeric_limits<SIZE_T>::max(), bytes));
    CHECK(try_allocation_bytes(128, 256, bytes));
    CHECK(bytes == 512);
    static_assert(maximum_capacity(0) == 0);
}

TEST_CASE("SandboxCore.SingleAllocation.Compact views slice and append self slices") {
    SingleAllocationEntityData owner;
    owner.add_defaulted(65);
    auto view{owner.get_view()};
    static_assert(std::is_same_v<decltype(view.view_locations()), soa::Vector3View<float>>);
    static_assert(sizeof(soa::Vector2View<float>) == 16);
    static_assert(sizeof(soa::Vector2ConstView<double>) == 16);
    static_assert(sizeof(soa::Vector3View<double>) == 16);
    static_assert(sizeof(soa::Vector3ConstView<float>) == 16);
    static_assert(std::is_trivially_copyable_v<soa::Vector3View<float>>);
    static_assert(std::is_convertible_v<soa::Vector3View<float>, soa::Vector3ConstView<float>>);
    static_assert(!std::is_convertible_v<soa::Vector3ConstView<float>, soa::Vector3View<float>>);
    auto vectors{view.view_locations()};
    auto const stride{vectors.byte_stride()};
    CHECK(stride == static_cast<SIZE_T>(owner.capacity()) * sizeof(float) + 192);
    soa::Vector2View<float> xy{vectors.xs().GetData(), stride, vectors.num()};
    xy.slice(2, 3).ys()[0] = 7.f;
    CHECK(vectors.ys()[2] == 7.f);
    CHECK(xy.right(0).xs().GetData() == vectors.xs().GetData() + vectors.num());
    CHECK(xy.right(0).ys().GetData() == vectors.ys().GetData() + vectors.num());
    auto tail{vectors.slice(1, 64).slice(2, 3)};
    CHECK(tail.byte_stride() == stride);
    CHECK(tail.zs().GetData() == vectors.zs().GetData() + 3);
    auto columns{tail.columns()};
    columns.zs[0] = 9.f;
    CHECK(vectors.zs()[3] == 9.f);
    soa::Vector3ConstView<float> readonly{tail};
    CHECK(readonly.zs()[0] == 9.f);
    soa::Vector3View<double> empty;
    CHECK(empty.slice(0, 0).zs().GetData() == nullptr);
    static_assert(std::is_same_v<decltype(view.view_velocities()), soa::Vector3View<float>>);
    static_assert(std::is_convertible_v<SingleAllocationEntityData::View, SingleAllocationEntityData::ConstView>);
    static_assert(!std::is_convertible_v<SingleAllocationEntityData::ConstView, SingleAllocationEntityData::View>);
    for (int32 row{}; row < 65; ++row) {
        view.healths()[row] = row;
        view.view_locations().xs()[row] = static_cast<float>(row);
    }
    auto slice{view.slice(1, 64)};
    owner.reserve(256);
    slice = owner.slice(1, 64);
    CHECK(slice.view_locations().xs()[63] == 64.f);
    SingleAllocationEntityData::ConstView const_view{slice};
    static_assert(std::is_same_v<decltype(const_view.view_locations()), soa::Vector3ConstView<float>>);
    CHECK(const_view.view_locations().xs()[63] == 64.f);
    auto nested{slice.view_locations()};
    nested.xs()[0] = 123.f;
    CHECK(owner.get_view().view_locations().xs()[1] == 123.f);
    CHECK(const_view.columns().healths[0] == 1);
    CHECK(owner.append_from(const_view) == 65);
    CHECK(owner.get_view().healths()[128] == 64);
    CHECK(owner.append_from(owner) == 129);
    CHECK(owner.num() == 258);
    CHECK(owner.get_view().healths()[257] == 64);
    CHECK(owner.append_from(owner.left(0)) == 258);
    FMemorySingleEntityData other;
    CHECK(other.append_from(owner.slice(65, 64)) == 0);
    CHECK(other.get_view().healths()[63] == 64);
    owner.reset();
    CHECK(other.get_view().healths()[0] == 1);
}

TEST_CASE("SandboxCore.SingleAllocation.Bulk append copies every leaf across growth") {
    auto verify = []<typename Owner>() {
        Owner source;
        source.add_defaulted(129);
        each_leaf(source.get_view(), [](auto column) {
            for (int32 row{}; row < column.Num(); ++row) {
                FMemory::Memset(&column[row], row + 1, sizeof(column[row]));
            }
        });
        Owner destination;
        REQUIRE(destination.append_from(source) == 0);
        REQUIRE(destination.append_from(destination.slice(1, 128)) == 129);
        REQUIRE(destination.num() == 257);
        each_leaf(destination.get_const_view(), [](auto column) {
            using Element = std::remove_cvref_t<decltype(column[0])>;
            CHECK(reinterpret_cast<UPTRINT>(column.GetData()) % std::max(SIZE_T{64}, alignof(Element)) == 0);
            for (int32 row{}; row < column.Num(); ++row) {
                auto const expected{static_cast<uint8>(row < 129 ? row + 1 : row - 127)};
                auto const* bytes{reinterpret_cast<uint8 const*>(&column[row])};
                for (SIZE_T byte{}; byte < sizeof(Element); ++byte) {
                    REQUIRE(bytes[byte] == expected);
                }
            }
        });
    };
    verify.template operator()<SingleAllocationEntityData>();
    verify.template operator()<SingleAllocationAlignmentData>();
}

TEST_CASE("SandboxCore.SingleAllocation.Descending removal matches original row indices") {
    for (int32 count{}; count <= 10; ++count) {
        for (uint32 mask{}; mask < (1u << count); ++mask) {
            SingleAllocationEntityData owner;
            owner.add_defaulted(count);
            TArray<int32> indices;
            for (auto row{count - 1}; row >= 0; --row) {
                owner.get_view().healths()[row] = row;
                owner.get_view().view_locations().xs()[row] = static_cast<float>(row);
                if (mask & (1u << row)) {
                    indices.Add(row);
                }
            }
            auto const final_count{count - indices.Num()};
            TArray<int32> survivors;
            for (int32 row{final_count}; row < count; ++row) {
                if (!(mask & (1u << row))) {
                    survivors.Add(row);
                }
            }
            auto const capacity{owner.capacity()};
            owner.remove_at_swap(TConstArrayView<int32>{indices});
            REQUIRE(owner.num() == final_count);
            CHECK(owner.capacity() == capacity);
            int32 next{};
            for (int32 row{}; row < final_count; ++row) {
                auto const expected{mask & (1u << row) ? survivors[next++] : row};
                CHECK(owner.get_view().healths()[row] == expected);
                CHECK(owner.get_view().view_locations().xs()[row] == static_cast<float>(expected));
            }
        }
    }
    SingleAllocationEntityData rows;
    rows.add_defaulted(10);
    for (int32 row{}; row < 10; ++row) {
        rows.get_view().healths()[row] = row;
    }
    int32 const removed[]{4, 3, 2};
    rows.remove_at_swap(std::span<int32 const>{removed});
    int32 const expected[]{0, 1, 7, 8, 9, 5, 6};
    for (int32 row{}; row < 7; ++row) {
        CHECK(rows.get_view().healths()[row] == expected[row]);
    }
}
}
