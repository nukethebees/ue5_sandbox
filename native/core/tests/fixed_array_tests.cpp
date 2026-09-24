#include <sandbox/core/fixed_array.h>
#include <sandbox/core/fixed_storage.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <span>
#include <string>
#include <type_traits>
#include <utility>

namespace native_core_fixed_array_tests {
struct LifetimeTrackedValue {
    explicit LifetimeTrackedValue(std::int32_t const initial_value = 0)
        : value{initial_value} {
        ++live_count;
    }

    LifetimeTrackedValue(LifetimeTrackedValue const& other)
        : value{other.value} {
        ++live_count;
    }

    LifetimeTrackedValue(LifetimeTrackedValue&& other) noexcept
        : value{other.value} {
        ++live_count;
        other.value = -1;
    }

    ~LifetimeTrackedValue() { --live_count; }

    std::int32_t value{};
    inline static std::int32_t live_count{};
};

struct MoveOnlyValue {
    explicit MoveOnlyValue(std::int32_t const initial_value)
        : value{initial_value} {}

    MoveOnlyValue(MoveOnlyValue const&) = delete;
    auto operator=(MoveOnlyValue const&) -> MoveOnlyValue& = delete;

    MoveOnlyValue(MoveOnlyValue&& other) noexcept
        : value{other.value} {
        other.value = -1;
    }

    auto operator=(MoveOnlyValue&& other) noexcept -> MoveOnlyValue& {
        value = other.value;
        other.value = -1;
        return *this;
    }

    std::int32_t value{};
};

struct alignas(64) OverAlignedValue {
    explicit OverAlignedValue(std::int32_t const initial_value)
        : value{initial_value} {}

    std::int32_t value{};
};

struct ResultHandle {
    std::int32_t index{};
    std::int32_t generation{};
};

static_assert(std::is_trivially_copyable_v<ResultHandle>);
static_assert(std::is_trivially_destructible_v<ResultHandle>);

auto write_results(std::span<ResultHandle> const output,
                   std::int32_t const result_count,
                   std::int32_t const generation) -> std::int32_t {
    EXPECT_GE(result_count, 0);
    EXPECT_LE(result_count, static_cast<std::int32_t>(output.size()));

    for (std::int32_t index{}; index < result_count; ++index) {
        output[index] = {.index = index, .generation = generation};
    }

    return result_count;
}

void expect_results(ml::FixedArray<ResultHandle, 8> const& values,
                    std::int32_t const expected_count,
                    std::int32_t const expected_generation) {
    ASSERT_EQ(values.num(), expected_count);

    std::int32_t expected_index{};
    for (ResultHandle const& result : values) {
        EXPECT_EQ(result.index, expected_index);
        EXPECT_EQ(result.generation, expected_generation);
        EXPECT_EQ(values[expected_index].index, expected_index);
        EXPECT_EQ(values[expected_index].generation, expected_generation);
        ++expected_index;
    }
}

TEST(NativeCoreFixedStorage, DefersLifetimeAndPreservesAlignment) {
    EXPECT_EQ(LifetimeTrackedValue::live_count, 0);

    {
        ml::TFixedStorage<LifetimeTrackedValue, 3> storage;
        ml::TFixedStorage<OverAlignedValue, 1> aligned_storage;
        ml::TFixedStorage<std::int32_t, 0> empty_storage;

        static_assert(ml::TFixedStorage<LifetimeTrackedValue, 3>::capacity() == 3);
        static_assert(ml::TFixedStorage<std::int32_t, 0>::capacity() == 0);
        EXPECT_EQ(LifetimeTrackedValue::live_count, 0);

        auto& first{storage.construct_at(0, 10)};
        auto& second{storage.construct_at(1, 20)};
        auto& aligned{aligned_storage.construct_at(0, 42)};

        EXPECT_EQ(LifetimeTrackedValue::live_count, 2);
        EXPECT_EQ(storage.data(), &first);
        EXPECT_EQ(second.value, 20);
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(aligned_storage.data()) %
                      alignof(OverAlignedValue),
                  0);
        EXPECT_EQ(aligned.value, 42);
        EXPECT_EQ(empty_storage.capacity(), 0);

        aligned_storage.destroy_at(0);
        storage.destroy_at(1);
        storage.destroy_at(0);
    }

    EXPECT_EQ(LifetimeTrackedValue::live_count, 0);
}

TEST(NativeCoreFixedArray, SupportsZeroCapacityInitializersAndNativeViews) {
    ml::FixedArray<std::int32_t, 0> empty;
    ml::FixedArray<std::int32_t, 4> values{10, 20};

    static_assert(ml::FixedArray<std::int32_t, 4>::capacity() == 4);
    static_assert(ml::FixedArray<std::int32_t, 0>::capacity() == 0);
    static_assert(std::is_same_v<decltype(values.num()), std::int32_t>);

    empty.reserve(0);
    EXPECT_TRUE(empty.is_empty());
    EXPECT_TRUE(empty.is_full());

    std::span<std::int32_t> active{values};
    auto const capacity{values.capacity_view()};
    EXPECT_EQ(active.size(), 2);
    EXPECT_EQ(active.data(), values.data());
    EXPECT_EQ(capacity.size(), 4);

    capacity[2] = 30;
    capacity[3] = 40;
    values.set_num_uninitialised(4);
    EXPECT_EQ(values.last(), 40);
}

TEST(NativeCoreFixedArray, AddsEmplacesAndResizesItsActiveRange) {
    ml::FixedArray<std::string, 4> values;

    EXPECT_EQ(values.add("first"), 0);
    auto& second{values.emplace_back("second")};
    EXPECT_EQ(values.add(std::string{"third"}), 2);
    EXPECT_EQ(values.data() + 1, &second);

    ml::FixedArray<std::int32_t, 4> defaults;
    defaults.add_defaulted(3);
    defaults[1] = 42;
    defaults.set_num(2);
    defaults.set_num(4);

    EXPECT_EQ(values.num(), 3);
    EXPECT_EQ(values[0], "first");
    EXPECT_EQ(second, "second");
    EXPECT_EQ(defaults[0], 0);
    EXPECT_EQ(defaults[1], 42);
    EXPECT_EQ(defaults[2], 0);
    EXPECT_EQ(defaults[3], 0);
}

TEST(NativeCoreFixedArray, CopiesMovesAndSupportsMoveOnlyValues) {
    ml::FixedArray<std::string, 3> source{"alpha", "beta"};
    ml::FixedArray<std::string, 3> copied{source};
    ml::FixedArray<std::string, 3> moved{std::move(source)};
    ml::FixedArray<std::string, 3> assigned;

    assigned = copied;
    assigned = std::move(moved);

    // NOLINTNEXTLINE(bugprone-use-after-move): Verify the defined moved-from state.
    EXPECT_TRUE(source.is_empty());
    // NOLINTNEXTLINE(bugprone-use-after-move): Verify the defined moved-from state.
    EXPECT_TRUE(moved.is_empty());
    EXPECT_EQ(copied.num(), 2);
    EXPECT_EQ(assigned[0], "alpha");
    EXPECT_EQ(assigned[1], "beta");

    ml::FixedArray<MoveOnlyValue, 2> move_only;
    move_only.emplace_back(10);
    move_only.emplace_back(20);
    ml::FixedArray<MoveOnlyValue, 2> moved_only{std::move(move_only)};

    // NOLINTNEXTLINE(bugprone-use-after-move): Verify the defined moved-from state.
    EXPECT_TRUE(move_only.is_empty());
    EXPECT_EQ(moved_only[0].value, 10);
    EXPECT_EQ(moved_only[1].value, 20);
}

TEST(NativeCoreFixedArray, DestroysRemovedElementsAndReusesStorage) {
    EXPECT_EQ(LifetimeTrackedValue::live_count, 0);

    {
        ml::FixedArray<LifetimeTrackedValue, 4> values;
        values.emplace_back(10);
        values.emplace_back(20);
        values.emplace_back(30);
        EXPECT_EQ(LifetimeTrackedValue::live_count, 3);

        values.pop();
        values.set_num(1);
        EXPECT_EQ(LifetimeTrackedValue::live_count, 1);

        values.reset();
        EXPECT_TRUE(values.is_empty());
        EXPECT_EQ(LifetimeTrackedValue::live_count, 0);
    }

    ml::FixedArray<std::int32_t, 8> values;
    values.reserve(8);
    for (std::int32_t iteration{}; iteration < 64; ++iteration) {
        values.set_num(8);
        for (std::int32_t index{}; index < values.num(); ++index) {
            values[index] = iteration + index;
        }
        values.set_num(2);
        values.set_num(8);
        EXPECT_EQ(values[0], iteration);
        EXPECT_EQ(values[1], iteration + 1);
    }
}

TEST(NativeCoreFixedArray, SupportsUninitialisedQueryResultBuffers) {
    ml::FixedArray<ResultHandle, 8> results;

    results.set_num_uninitialised(write_results(results.capacity_view(), 3, 10));
    expect_results(results, 3, 10);

    results.set_num_uninitialised(write_results(results.capacity_view(), 0, 11));
    EXPECT_TRUE(results.is_empty());
    EXPECT_EQ(results.begin(), results.end());

    results.set_num_uninitialised(write_results(results.capacity_view(), results.capacity(), 12));
    EXPECT_TRUE(results.is_full());
    expect_results(results, results.capacity(), 12);

    results.set_num_uninitialised(2);
    results.set_num_uninitialised(write_results(results.capacity_view(), 5, 13));
    expect_results(results, 5, 13);

    for (std::int32_t iteration{}; iteration < 1024; ++iteration) {
        auto const count{iteration % (results.capacity() + 1)};
        auto const generation{100 + iteration};
        results.set_num_uninitialised(write_results(results.capacity_view(), count, generation));
        expect_results(results, count, generation);
    }
}

TEST(NativeCoreFixedArray, CopiesAndMovesUninitialisedQueryResults) {
    ml::FixedArray<ResultHandle, 8> source;
    source.set_num_uninitialised(write_results(source.capacity_view(), 4, 27));

    ml::FixedArray<ResultHandle, 8> copied{source};
    ml::FixedArray<ResultHandle, 8> moved{std::move(source)};

    // NOLINTNEXTLINE(bugprone-use-after-move): Verify the defined moved-from state.
    EXPECT_TRUE(source.is_empty());
    expect_results(copied, 4, 27);
    expect_results(moved, 4, 27);
}
}
