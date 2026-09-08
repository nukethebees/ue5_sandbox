#include <SandboxCore/array_utils.h>
#include <SbxCoreExperiments/soa_test_support.h>
#include <SbxCoreExperiments/soa_types.h>

#include "TestHarness.h"

#include <array>
#include <limits>
#include <type_traits>

namespace ml::single_allocation_tests {
using namespace single_allocation_experiment;

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
static_assert(std::is_same_v<EntityData::View, SingleAllocationEntityData::View>);
static_assert(std::is_same_v<EntityData::ConstView, SingleAllocationEntityData::ConstView>);
static_assert(sizeof(SingleAllocationEntityData) == sizeof(void*) + 2 * sizeof(int32));

TEST_CASE("SandboxCore.SingleAllocation.Empty and end views preserve column pointers") {
    SingleAllocationAlignmentData rows;
    auto verify_views = [&](int32 const count) {
        auto full{rows.get_view()};
        auto const empty{rows.get_view(count, 0)};
        auto const& const_rows{rows};
        auto const const_full{const_rows.get_view()};
        auto const const_empty{const_rows.get_view(count, 0)};
        static_assert(std::is_same_v<decltype(full.nested.xs.GetData()), float*>);
        static_assert(std::is_same_v<decltype(const_full.nested.xs.GetData()), float const*>);
        CHECK(full.num() == count);
        CHECK(empty.num() == 0);
        CHECK(const_full.num() == count);
        CHECK(const_empty.num() == 0);
        CHECK(full.nested.xs.GetData() == const_full.nested.xs.GetData());
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
    each_leaf(rows.get_view(), [](auto column) {
        CHECK(column.Num() == 0);
        CHECK(column.GetData() == nullptr);
    });
    rows.reserve(0);
    rows.add_uninitialised(0);
    rows.add_defaulted(0);
    rows.set_num(0);
    rows.remove_at_swap(0, 0);
    CHECK(rows.capacity() == 0);

    for (int32 const count : {1, 63, 64, 65, 127, 128, 129}) {
        CAPTURE(count);
        SingleAllocationAlignmentData values;
        values.reserve(count);
        CHECK(values.capacity() == ((count + 63) / 64) * 64);
        CHECK(values.num() == 0);
        values.set_num(count);
        auto view{values.get_view()};
        auto const base{reinterpret_cast<UPTRINT>(view.bytes.GetData())};
        CHECK(base % values.allocation_alignment == 0);
        UPTRINT end{base};
        each_leaf(view, [&](auto column) {
            using Element = std::remove_cvref_t<decltype(column[0])>;
            auto const address{reinterpret_cast<UPTRINT>(column.GetData())};
            CHECK(column.Num() == count);
            CHECK(address % alignof(Element) == 0);
            CHECK(address % 64 == 0);
            CHECK(address >= end);
            end = address + static_cast<SIZE_T>(values.capacity()) * sizeof(Element);
            CHECK(end <= base + values.allocated_bytes());
        });
        auto const blocks{static_cast<SIZE_T>(values.capacity() / 64)};
        CHECK(reinterpret_cast<UPTRINT>(view.aligned32.GetData()) - base == blocks * values.aligned32_block_offset);
        CHECK(reinterpret_cast<UPTRINT>(view.nested.xs.GetData()) - base == blocks * values.nested_xs_block_offset);
        CHECK(reinterpret_cast<UPTRINT>(view.aligned64.GetData()) - base == blocks * values.aligned64_block_offset);
        CHECK(reinterpret_cast<UPTRINT>(view.aligned256.GetData()) - base == blocks * values.aligned256_block_offset);
        CHECK(values.allocated_bytes() == blocks * values.block_bytes);
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
        view = values.get_view();
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
    each_leaf(values.get_const_view(), [&](auto column) {
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
        auto* const old_pointer{values.get_view().entity_handles.GetData()};
        values.add_uninitialised(1);
        CHECK(values.capacity() % 64 == 0);
        if (old_capacity == values.capacity()) {
            REQUIRE(values.get_view().entity_handles.GetData() == old_pointer);
        } else {
            ++growths;
        }
        int32 columns{};
        each_leaf(values.get_view(), [&](auto column) {
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
    auto view{values.get_view()};
    for (int32 index{}; index < values.num(); ++index) {
        CHECK(view.healths[index] == 0);
        CHECK(view.entity_handles[index].index == -1);
        CHECK(view.locations.xs[index] == 0.f);
    }
    view.locations.xs[2] = 12.f;
    view.healths[2] = 42;
    values.get_view(2, 3).healths[1] = 43;
    SingleAllocationEntityData const& const_values{values};
    static_assert(std::is_same_v<decltype(const_values.get_view().healths[0]), int32 const&>);
    CHECK(const_values.get_view().locations.xs[2] == 12.f);
    CHECK(const_values.get_const_view(2, 3).healths[1] == 43);
    CHECK(values.slice(2, 1).healths[0] == 42);
    CHECK(values.left(4).healths[3] == 43);
    CHECK(values.right(63).healths[0] == 42);
    CHECK(values.get_view(values.num(), 0).num() == 0);

    auto* const pointer{view.healths.GetData()};
    values.set_num(3, EAllowShrinking::Yes);
    CHECK(values.capacity() == 128);
    values.set_num(65);
    CHECK(values.get_view().healths.GetData() == pointer);
    CHECK(values.get_view().healths[2] == 42);
    CHECK(values.get_view().healths[3] == 0);
    values.set_num(129);
    CHECK(values.get_view().healths[2] == 42);
    CHECK(values.get_view().entity_handles[128].index == -1);
    auto const capacity{values.capacity()};
    values.reset();
    CHECK(values.is_empty());
    CHECK(values.capacity() == capacity);
    values.add_defaulted(1);
    CHECK(values.get_view().healths[0] == 0);
    CHECK(values.get_view().entity_handles[0].index == -1);
    ml::fill(values.get_view().healths, 123);
    CHECK(values.get_const_view().healths[0] == 123);
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
            fill(values.get_view());
            fill(baseline.get_view());
            values.remove_at_swap(index, count, EAllowShrinking::Yes);
            baseline.remove_at_swap(index, count, EAllowShrinking::No);
            CHECK(values.capacity() == 64);
            CHECK(values.num() == baseline.num());
            std::array<void const*, 53> expected{};
            int32 leaf{};
            each_leaf(baseline.get_const_view(), [&](auto column) { expected[leaf++] = column.GetData(); });
            leaf = 0;
            each_leaf(values.get_const_view(), [&](auto column) {
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
    first.get_view().nested.ys[64] = 19.f;
    auto* const pointer{first.get_view().bytes.GetData()};
    auto const capacity{first.capacity()};
    SingleAllocationAlignmentData second{std::move(first)};
    CHECK(first.num() == 0);
    CHECK(first.capacity() == 0);
    CHECK(first.get_view().bytes.GetData() == nullptr);
    CHECK(second.get_view().bytes.GetData() == pointer);
    first.add_defaulted(1);
    first = std::move(second);
    CHECK(first.capacity() == capacity);
    CHECK(first.get_view().bytes.GetData() == pointer);
    CHECK(first.get_view().nested.ys[64] == 19.f);
    CHECK(second.capacity() == 0);
    auto* const self{&first};
    first = std::move(*self);
    CHECK(first.num() == 65);
    second.add_defaulted(1);
    CHECK(second.get_view().aligned256[0].value == 256);
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

}
