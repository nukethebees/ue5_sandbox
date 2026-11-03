#include <ioj/linked_vector.h>
#include <ioj/memory/bump_resource.h>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <ranges>
#include <stdexcept>
#include <vector>

namespace ioj::tests {
static_assert(std::bidirectional_iterator<LinkedVector<int>::iterator>);
static_assert(std::bidirectional_iterator<LinkedVector<int>::const_iterator>);
static_assert(std::same_as<std::iter_reference_t<LinkedVector<int>::const_iterator>, int const&>);
static_assert(!std::is_copy_constructible_v<LinkedVector<int>>);

TEST(LinkedVector, IteratesMultipleSegmentsAndPreservesAddresses) {
    LinkedVector<int> values;
    std::vector<int*> addresses;
    for (int i{}; i < 100; ++i) {
        addresses.push_back(&values.emplace_back(i));
    }
    int expected{};
    for (auto& value : values) {
        EXPECT_EQ(value, expected);
        EXPECT_EQ(&value, addresses[expected]);
        ++expected;
    }
    EXPECT_EQ(expected, 100);
    auto it{values.end()};
    for (int i{99}; i >= 0; --i) {
        EXPECT_EQ(*--it, i);
    }
    EXPECT_EQ(it, values.begin());
    LinkedVector<int>::const_iterator const_it{values.begin()};
    EXPECT_EQ(const_it, values.begin());
    LinkedVector<int> other;
    EXPECT_NE(values.end(), other.end());
}

TEST(LinkedVector, ReserveClearAndRepeatedGrowthReuseEverySegment) {
    LinkedVector<int> values;
    values.reserve(3);
    values.reserve(10);
    EXPECT_EQ(values.begin(), values.end());
    for (int repeat{}; repeat < 4; ++repeat) {
        for (int i{}; i < 20; ++i) {
            values.emplace_back(i);
        }
        EXPECT_EQ(std::ranges::distance(values.begin(), values.end()), 20);
        EXPECT_EQ(values.back(), 19);
        auto const capacity{values.capacity()};
        values.clear();
        EXPECT_EQ(values.capacity(), capacity);
        EXPECT_TRUE(values.empty());
        EXPECT_EQ(values.begin(), values.end());
    }
}

TEST(LinkedVector, PopsAcrossEmptyAndReservedSegmentsAndAppendsAgain) {
    LinkedVector<int> values;
    for (int i{}; i < 17; ++i) {
        values.emplace_back(i);
    }
    values.reserve(100);
    values.reserve(200);
    for (int i{16}; i >= 0; --i) {
        EXPECT_EQ(values.back(), i);
        values.pop_back();
        EXPECT_EQ(std::ranges::distance(values.begin(), values.end()), i);
    }
    EXPECT_EQ(values.begin(), values.end());
    for (int i{}; i < 200; ++i) {
        values.emplace_back(i);
    }
    EXPECT_EQ(values.back(), 199);
    EXPECT_EQ(values.capacity(), 200);
}

struct TrackedLinkedValue {
    explicit TrackedLinkedValue(int& live, int value)
        : live_{live}
        , value{value} {
        if (value == 99) {
            throw std::runtime_error{"construction failure"};
        }
        ++live_;
    }
    ~TrackedLinkedValue() { --live_; }
    int& live_;
    int value;
};

TEST(LinkedVector, DestroysExactlyOnceAndTransfersResourceOnMove) {
    int live{};
    memory::BumpResource resource{4096};
    {
        LinkedVector<TrackedLinkedValue> source{resource};
        source.emplace_back(live, 1);
        source.emplace_back(live, 2);
        EXPECT_THROW(source.emplace_back(live, 99), std::runtime_error);
        EXPECT_EQ(source.size(), 2);
        EXPECT_EQ(live, 2);
        auto* first{&*source.begin()};
        LinkedVector<TrackedLinkedValue> moved{std::move(source)};
        // NOLINTNEXTLINE(bugprone-use-after-move): Verify the defined moved-from state.
        EXPECT_TRUE(source.empty());
        EXPECT_EQ(&*moved.begin(), first);
        LinkedVector<TrackedLinkedValue> target;
        target.emplace_back(live, 3);
        target = std::move(moved);
        EXPECT_EQ(live, 2);
        EXPECT_EQ(&*target.begin(), first);
        target.pop_back();
        EXPECT_EQ(live, 1);
        target.clear();
        EXPECT_EQ(live, 0);
        target.emplace_back(live, 4);
        source.emplace_back(live, 5);
    }
    EXPECT_EQ(live, 0);
}

struct alignas(256) AlignedLinkedValue {
    int value{};
};

TEST(LinkedVector, AlignsElementsAndReusesSegmentsAfterClear) {
    memory::BumpResource resource{4096};
    LinkedVector<AlignedLinkedValue> values{resource};
    std::vector<AlignedLinkedValue*> addresses;
    for (int i{}; i < 100; ++i) {
        auto& value{values.emplace_back(i)};
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(&value) % alignof(AlignedLinkedValue), 0);
        addresses.push_back(&value);
    }
    EXPECT_EQ(values.size(), 100);
    EXPECT_EQ(std::ranges::distance(values.begin(), values.end()), 100);
    values.clear();
    for (int i{}; i < 100; ++i) {
        EXPECT_EQ(&values.emplace_back(i), addresses[i]);
    }
    values.release();
    resource.reset();
    EXPECT_EQ(values.emplace_back(7).value, 7);
}

TEST(LinkedVector, RejectsUnrepresentableReserveAndPreservesValuesOnAllocationFailure) {
    LinkedVector<int> empty{*std::pmr::null_memory_resource()};
    EXPECT_THROW(empty.reserve(1), std::bad_alloc);
    EXPECT_THROW(empty.reserve(std::numeric_limits<std::size_t>::max()), std::bad_alloc);
    EXPECT_THROW(empty.emplace_back(1), std::bad_alloc);
    EXPECT_TRUE(empty.empty());
    EXPECT_EQ(empty.capacity(), 0);

    std::array<std::byte, 1024> buffer;
    std::pmr::monotonic_buffer_resource resource{
        buffer.data(), buffer.size(), std::pmr::null_memory_resource()};
    LinkedVector<int> values{resource};
    bool exhausted{};
    for (int i{}; i < 1024; ++i) {
        try {
            values.emplace_back(i);
        } catch (std::bad_alloc const&) {
            exhausted = true;
            break;
        }
    }
    ASSERT_TRUE(exhausted);
    auto const count{values.size()};
    ASSERT_GT(count, 0);
    EXPECT_EQ(values.back(), count - 1);
    EXPECT_EQ(std::ranges::distance(values.begin(), values.end()), count);
    values.clear();
    for (std::size_t i{}; i < count; ++i) {
        values.emplace_back(static_cast<int>(i));
    }
    EXPECT_EQ(values.size(), count);
}
}
