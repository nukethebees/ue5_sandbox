#include <ioj/linked_vector.h>

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
        EXPECT_EQ(std::ranges::distance(values), 20);
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
        EXPECT_EQ(std::ranges::distance(values), i);
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
    memory::BoundedResource resource{4096};
    {
        LinkedVector<TrackedLinkedValue> source{resource};
        source.emplace_back(live, 1);
        source.emplace_back(live, 2);
        EXPECT_THROW(source.emplace_back(live, 99), std::runtime_error);
        EXPECT_EQ(source.size(), 2);
        EXPECT_EQ(live, 2);
        auto* first{&*source.begin()};
        LinkedVector<TrackedLinkedValue> moved{std::move(source)};
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

TEST(LinkedVector, AlignsElementsAndStopsAtFixedBudgetWithoutLosingValues) {
    memory::BoundedResource resource{4096};
    LinkedVector<AlignedLinkedValue> values{resource};
    int count{};
    while (auto* value{values.try_emplace_back(count)}) {
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(value) % alignof(AlignedLinkedValue), 0);
        ++count;
    }
    ASSERT_GT(count, 0);
    EXPECT_EQ(values.size(), count);
    EXPECT_EQ(std::ranges::distance(values), count);
    auto const used{resource.used_bytes()};
    EXPECT_EQ(values.try_emplace_back(100), nullptr);
    EXPECT_EQ(resource.used_bytes(), used);
    int expected{};
    for (auto const& value : values) {
        EXPECT_EQ(value.value, expected++);
    }
    values.clear();
    for (int i{}; i < count; ++i) {
        ASSERT_NE(values.try_emplace_back(i), nullptr);
    }
    EXPECT_EQ(resource.used_bytes(), used);
    values.release();
    resource.reset();
    EXPECT_NE(values.try_emplace_back(7), nullptr);
}

TEST(LinkedVector, RejectsUnrepresentableReserveAndHandlesUpstreamFailure) {
    LinkedVector<int> values{*std::pmr::null_memory_resource()};
    EXPECT_FALSE(values.try_reserve(1));
    EXPECT_FALSE(values.try_reserve(std::numeric_limits<std::size_t>::max()));
    EXPECT_EQ(values.try_emplace_back(1), nullptr);
    EXPECT_TRUE(values.empty());
    EXPECT_EQ(values.capacity(), 0);
}

TEST(BoundedResource, ExactCapacityFailureAndReset) {
    memory::BoundedResource resource{64};
    EXPECT_NE(resource.try_allocate(64, 64), nullptr);
    EXPECT_EQ(resource.used_bytes(), 64);
    EXPECT_EQ(resource.try_allocate(1, 1), nullptr);
    EXPECT_EQ(resource.used_bytes(), 64);
    resource.reset();
    EXPECT_NE(resource.try_allocate(1, 1), nullptr);
    EXPECT_EQ(resource.try_allocate(64, 64), nullptr);
    EXPECT_EQ(resource.used_bytes(), 1);
    memory::BoundedResource empty{0};
    EXPECT_EQ(empty.try_allocate(1, 1), nullptr);
}
}
