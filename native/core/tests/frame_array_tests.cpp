#include <sandbox/core/frame_array.h>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <memory_resource>
#include <span>
#include <string>
#include <type_traits>
#include <utility>

namespace native_core_frame_array_tests {
struct TrackedValue {
    explicit TrackedValue(std::int32_t const initial_value = 0)
        : value{initial_value} {
        ++live_count;
    }

    TrackedValue(TrackedValue const& other)
        : value{other.value} {
        ++copy_count;
        ++live_count;
    }

    TrackedValue(TrackedValue&& other) noexcept
        : value{other.value} {
        ++move_count;
        ++live_count;
        other.value = -1;
    }

    auto operator=(TrackedValue const& other) -> TrackedValue& {
        value = other.value;
        return *this;
    }

    auto operator=(TrackedValue&& other) noexcept -> TrackedValue& {
        value = other.value;
        other.value = -1;
        return *this;
    }

    ~TrackedValue() {
        ++destruction_count;
        --live_count;
    }

    static void reset_counts() {
        ASSERT_EQ(live_count, 0);
        copy_count = 0;
        move_count = 0;
        destruction_count = 0;
    }

    std::int32_t value{};
    inline static std::int32_t live_count{};
    inline static std::int32_t copy_count{};
    inline static std::int32_t move_count{};
    inline static std::int32_t destruction_count{};
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
TEST(NativeCoreFrameArray, BorrowsStorageAndHasFixedIdentity) {
    using Array = ml::FrameArray<std::int32_t>;

    static_assert(!std::is_default_constructible_v<Array>);
    static_assert(!std::is_copy_constructible_v<Array>);
    static_assert(!std::is_move_constructible_v<Array>);
    static_assert(std::is_same_v<decltype(std::declval<Array const&>().num()), std::int32_t>);

    std::array<std::byte, 1024> backing{};
    std::pmr::monotonic_buffer_resource resource{backing.data(), backing.size()};
    Array values{&resource};
    auto* const identity{std::addressof(values)};

    EXPECT_TRUE(values.is_empty());
    EXPECT_EQ(identity, std::addressof(values));
    EXPECT_EQ(values.view().size(), 0);
}

TEST(NativeCoreFrameArray, AddsEmplacesAndExposesNativeViews) {
    std::pmr::unsynchronized_pool_resource resource;
    ml::FrameArray<TrackedValue> values{&resource};
    values.reserve(3);
    TrackedValue copied_source{10};
    TrackedValue moved_source{20};

    auto& copied{values.add(copied_source)};
    auto& moved{values.add(std::move(moved_source))};
    auto& emplaced{values.emplace(30)};
    std::span<TrackedValue> mutable_view{values};
    std::span<TrackedValue const> const_view{std::as_const(values)};

    EXPECT_EQ(&copied, values.data());
    EXPECT_EQ(&moved, values.data() + 1);
    EXPECT_EQ(&emplaced, values.data() + 2);
    EXPECT_EQ(copied_source.value, 10);
    // NOLINTNEXTLINE(bugprone-use-after-move): Verify the defined moved-from state.
    EXPECT_EQ(moved_source.value, -1);
    EXPECT_EQ(TrackedValue::copy_count, 1);
    EXPECT_EQ(TrackedValue::move_count, 1);
    EXPECT_EQ(mutable_view.size(), 3);
    EXPECT_EQ(const_view[2].value, 30);
}

TEST(NativeCoreFrameArray, ReservesReusesAndIteratesWithConstCorrectness) {
    std::pmr::unsynchronized_pool_resource resource;
    ml::FrameArray<std::int32_t> values{&resource};
    values.reserve(4);
    values.add(1);
    auto* const reserved_data{values.data()};
    values.add(2);
    values.add(3);
    values.add(4);

    for (std::int32_t& value : values) {
        value *= 10;
    }
    auto const& const_values{values};
    std::int32_t sum{};
    for (std::int32_t const value : const_values) {
        sum += value;
    }

    values.clear();
    auto& reused{values.add(50)};
    EXPECT_EQ(reserved_data, values.data());
    EXPECT_EQ(&reused, reserved_data);
    EXPECT_EQ(sum, 100);
}

TEST(NativeCoreFrameArray, DestroysActiveValuesAndSupportsMoveOnlyValues) {
    TrackedValue::reset_counts();
    {
        std::pmr::unsynchronized_pool_resource resource;
        ml::FrameArray<TrackedValue> values{&resource};
        values.reserve(3);
        values.emplace(10);
        values.emplace(20);
        values.emplace(30);
        EXPECT_EQ(TrackedValue::live_count, 3);

        values.clear();
        EXPECT_EQ(TrackedValue::live_count, 0);
        EXPECT_EQ(TrackedValue::destruction_count, 3);
    }

    std::pmr::unsynchronized_pool_resource resource;
    ml::FrameArray<MoveOnlyValue> values{&resource};
    MoveOnlyValue source{10};
    values.add(std::move(source));
    values.emplace(20);

    // NOLINTNEXTLINE(bugprone-use-after-move): Verify the defined moved-from state.
    EXPECT_EQ(source.value, -1);
    EXPECT_EQ(values[0].value, 10);
    EXPECT_EQ(values[1].value, 20);
}

TEST(NativeCoreFrameArray, ResizesRemovesAndPreservesValuesThroughGrowth) {
    std::pmr::unsynchronized_pool_resource resource;
    ml::FrameArray<std::int32_t> values{&resource};
    values.set_num(3);
    values[0] = 10;
    values[1] = 20;
    values[2] = 30;
    values.remove_at_swap(1);

    ASSERT_EQ(values.num(), 2);
    EXPECT_EQ(values[0], 10);
    EXPECT_EQ(values[1], 30);

    for (std::int32_t index{}; index < 2048; ++index) {
        values.add(index);
    }
    EXPECT_EQ(values.num(), 2050);
    EXPECT_EQ(values[2], 0);
    EXPECT_EQ(values[2049], 2047);
}

TEST(NativeCoreFrameArray, AcceptsBorrowedStandardResources) {
    std::pmr::unsynchronized_pool_resource resource{std::pmr::new_delete_resource()};
    ml::FrameArray<std::string> values{&resource};

    values.reserve(2);
    values.emplace("alpha");
    values.add(std::string{"beta"});

    EXPECT_EQ(values.num(), 2);
    EXPECT_EQ(values[0], "alpha");
    EXPECT_EQ(values[1], "beta");
}
}
