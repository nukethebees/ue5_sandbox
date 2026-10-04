#include <sandbox/core/frame_array.h>
#include <sandbox/core/frame_memory_resource.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <memory_resource>
#include <new>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

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

class RecordingResource final : public std::pmr::memory_resource {
  public:
    struct Allocation {
        void* pointer{};
        std::size_t bytes{};
        std::size_t alignment{};
    };

    std::vector<Allocation> allocations;
    std::uint32_t outstanding{};
    bool fail_next{};
  private:
    auto do_allocate(std::size_t const bytes, std::size_t const alignment) -> void* override {
        if (std::exchange(fail_next, false)) {
            throw std::bad_alloc{};
        }

        auto* const pointer{std::pmr::new_delete_resource()->allocate(bytes, alignment)};
        allocations.push_back({pointer, bytes, alignment});
        ++outstanding;
        return pointer;
    }

    void do_deallocate(void* const pointer,
                       std::size_t const bytes,
                       std::size_t const alignment) override {
        auto const found{std::find_if(
            allocations.begin(), allocations.end(), [pointer](Allocation const& allocation) {
                return allocation.pointer == pointer;
            })};
        ASSERT_NE(found, allocations.end());
        EXPECT_EQ(found->bytes, bytes);
        EXPECT_EQ(found->alignment, alignment);
        found->pointer = nullptr;
        --outstanding;
        std::pmr::new_delete_resource()->deallocate(pointer, bytes, alignment);
    }

    auto do_is_equal(std::pmr::memory_resource const& other) const noexcept -> bool override {
        return this == &other;
    }
};

struct ThrowingValue {
    explicit ThrowingValue(std::int32_t const initial_value = 0)
        : value{initial_value} {
        if (constructions_before_throw == 0) {
            throw std::runtime_error{"Element construction failed"};
        }
        if (constructions_before_throw > 0) {
            --constructions_before_throw;
        }
        ++live_count;
    }

    ThrowingValue(ThrowingValue const& other)
        : ThrowingValue{other.value} {}

    ThrowingValue(ThrowingValue&& other) noexcept
        : value{std::exchange(other.value, -1)} {
        ++live_count;
    }

    ~ThrowingValue() { --live_count; }

    std::int32_t value{};
    inline static std::int32_t live_count{};
    inline static std::int32_t constructions_before_throw{-1};
};

struct ThrowingMove {
    ThrowingMove(ThrowingMove&&) noexcept(false);
};

struct ThrowingDestructor {
    ThrowingDestructor(ThrowingDestructor&&) noexcept;
    ~ThrowingDestructor() noexcept(false);
};

template <typename T>
concept FrameArrayElement = requires { typename ml::FrameArray<T>; };

template <typename Array>
concept CopiesElements = requires(
    Array& array, typename decltype(array.view())::element_type const& value) { array.add(value); };

template <typename Array>
concept ResizesElements = requires(Array& array) { array.set_num(1); };

template <typename Array>
concept RemovesElements = requires(Array& array) { array.remove_at_swap(0); };

TEST(NativeCoreFrameArray, BorrowsStorageAndHasFixedIdentity) {
    using Array = ml::FrameArray<std::int32_t>;

    static_assert(!std::is_default_constructible_v<Array>);
    static_assert(!std::is_copy_constructible_v<Array>);
    static_assert(!std::is_move_constructible_v<Array>);
    static_assert(std::is_same_v<decltype(std::declval<Array const&>().num()), std::uint32_t>);
    static_assert(std::is_same_v<decltype(std::declval<Array&>().begin()), std::int32_t*>);
    static_assert(
        std::is_same_v<decltype(std::declval<Array const&>().begin()), std::int32_t const*>);
    static_assert(FrameArrayElement<MoveOnlyValue>);
    static_assert(!FrameArrayElement<ThrowingMove>);
    static_assert(!FrameArrayElement<ThrowingDestructor>);
    static_assert(CopiesElements<Array>);
    static_assert(!CopiesElements<ml::FrameArray<MoveOnlyValue>>);
    static_assert(!ResizesElements<ml::FrameArray<MoveOnlyValue>>);
    static_assert(RemovesElements<ml::FrameArray<MoveOnlyValue>>);
    static_assert(!RemovesElements<ml::FrameArray<ThrowingValue>>);

    std::array<std::byte, 1024> backing{};
    std::pmr::monotonic_buffer_resource resource{backing.data(), backing.size()};
    Array values{&resource};
    auto* const identity{std::addressof(values)};

    EXPECT_TRUE(values.is_empty());
    EXPECT_EQ(identity, std::addressof(values));
    EXPECT_EQ(values.view().size(), 0);
    EXPECT_EQ(values.begin(), values.end());
    EXPECT_EQ(std::as_const(values).begin(), std::as_const(values).end());
}

TEST(NativeCoreFrameArray, AddsEmplacesAndExposesNativeViews) {
    TrackedValue::reset_counts();
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

TEST(NativeCoreFrameArray, MatchesAllocationSizesAndAlignmentsAndRetainsClearedStorage) {
    struct alignas(256) AlignedValue {
        std::int32_t value{};
    };

    RecordingResource resource;
    {
        ml::FrameArray<AlignedValue> values{&resource};
        values.reserve(0);
        values.clear();
        EXPECT_TRUE(resource.allocations.empty());

        values.reserve(3);
        ASSERT_EQ(resource.allocations.size(), 1);
        EXPECT_EQ(resource.allocations[0].bytes, 3 * sizeof(AlignedValue));
        EXPECT_EQ(resource.allocations[0].alignment, alignof(AlignedValue));
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(values.data()) % alignof(AlignedValue), 0);
        values.set_num(3);
        values[0].value = 42;
        values.emplace(7);
        EXPECT_EQ(values[0].value, 42);
        EXPECT_EQ(values[3].value, 7);
        EXPECT_EQ(resource.outstanding, 1);

        auto* const storage{values.data()};
        auto const allocation_count{resource.allocations.size()};
        values.clear();
        values.reserve(1);
        values.set_num(4);
        EXPECT_EQ(values.data(), storage);
        EXPECT_EQ(resource.allocations.size(), allocation_count);
        EXPECT_EQ(values[0].value, 0);
    }
    EXPECT_EQ(resource.outstanding, 0);
}

TEST(NativeCoreFrameArray, ShrinksAndRemovesExactlyTheActiveElements) {
    TrackedValue::reset_counts();
    RecordingResource resource;
    {
        ml::FrameArray<TrackedValue> values{&resource};
        values.set_num(4);
        auto* const storage{values.data()};
        values[3].value = 30;
        values.remove_at_swap(1);
        EXPECT_EQ(values[1].value, 30);
        EXPECT_EQ(TrackedValue::live_count, 3);
        values.remove_at_swap(2);
        values.set_num(1);
        EXPECT_EQ(TrackedValue::live_count, 1);
        values.set_num(3);
        EXPECT_EQ(values[1].value, 0);
        EXPECT_EQ(values[2].value, 0);
        EXPECT_EQ(values.data(), storage);
        values.clear();
        EXPECT_EQ(TrackedValue::live_count, 0);
        values.emplace(99);
    }
    EXPECT_EQ(TrackedValue::live_count, 0);
    EXPECT_EQ(resource.outstanding, 0);

    ml::FrameArray<int> scalars{&resource};
    scalars.set_num(2);
    EXPECT_EQ(scalars[0], 0);
    EXPECT_EQ(scalars[1], 0);
    scalars[1] = 99;
    scalars.set_num(1);
    scalars.set_num(3);
    EXPECT_EQ(scalars[1], 0);
    EXPECT_EQ(scalars[2], 0);
}

TEST(NativeCoreFrameArray, UninitialisedGrowthLeavesNewElementsForCallerConstruction) {
    for (bool const reserve_spare : {false, true}) {
        TrackedValue::reset_counts();
        RecordingResource resource;
        {
            ml::FrameArray<TrackedValue> values{&resource};
            values.reserve(reserve_spare ? 3 : 1);
            values.emplace(42);
            auto const allocation_count{resource.allocations.size()};

            values.set_num_uninitialised(3);
            EXPECT_EQ(values.num(), 3);
            EXPECT_EQ(TrackedValue::live_count, 1);
            EXPECT_EQ(values[0].value, 42);
            EXPECT_EQ(resource.allocations.size(), allocation_count + (reserve_spare ? 0 : 1));

            std::construct_at(values.data() + 1, 17);
            std::construct_at(values.data() + 2, 99);
            EXPECT_EQ(values[1].value, 17);
            EXPECT_EQ(values[2].value, 99);
            EXPECT_EQ(TrackedValue::live_count, 3);
        }
        EXPECT_EQ(TrackedValue::live_count, 0);
        EXPECT_EQ(resource.outstanding, 0);
    }
}

TEST(NativeCoreFrameArray, UninitialisedResizeShrinksAndReusesStorage) {
    TrackedValue::reset_counts();
    RecordingResource resource;
    {
        ml::FrameArray<TrackedValue> values{&resource};
        values.set_num_uninitialised(0);
        EXPECT_TRUE(values.is_empty());
        EXPECT_EQ(values.data(), nullptr);
        EXPECT_TRUE(resource.allocations.empty());

        values.reserve(3);
        values.emplace(10);
        values.emplace(20);
        values.emplace(30);
        auto* const storage{values.data()};

        values.set_num_uninitialised(3);
        EXPECT_EQ(values.num(), 3);
        EXPECT_EQ(TrackedValue::live_count, 3);
        EXPECT_EQ(TrackedValue::destruction_count, 0);
        EXPECT_EQ(values[1].value, 20);
        EXPECT_EQ(values[2].value, 30);

        values.set_num_uninitialised(1);
        EXPECT_EQ(values.num(), 1);
        EXPECT_EQ(values[0].value, 10);
        EXPECT_EQ(TrackedValue::live_count, 1);
        EXPECT_EQ(TrackedValue::destruction_count, 2);

        values.set_num_uninitialised(3);
        EXPECT_EQ(TrackedValue::live_count, 1);
        std::construct_at(values.data() + 1, 40);
        std::construct_at(values.data() + 2, 50);
        EXPECT_EQ(values[0].value, 10);
        EXPECT_EQ(values[1].value, 40);
        EXPECT_EQ(values[2].value, 50);

        values.set_num_uninitialised(0);
        EXPECT_TRUE(values.is_empty());
        EXPECT_EQ(TrackedValue::live_count, 0);
        EXPECT_EQ(TrackedValue::destruction_count, 5);
        EXPECT_EQ(values.data(), storage);
        EXPECT_EQ(resource.allocations.size(), 1);
        EXPECT_EQ(resource.outstanding, 1);
    }
    EXPECT_EQ(TrackedValue::destruction_count, 5);
    EXPECT_EQ(resource.outstanding, 0);
}

TEST(NativeCoreFrameArray, UninitialisedResizeSupportsNonDefaultConstructibleMoveOnlyElements) {
    RecordingResource resource;
    {
        ml::FrameArray<MoveOnlyValue> values{&resource};
        values.set_num_uninitialised(1);
        EXPECT_EQ(values.num(), 1);
        std::construct_at(values.data(), 42);

        values.set_num_uninitialised(3);
        EXPECT_EQ(values.num(), 3);
        EXPECT_EQ(values[0].value, 42);
        std::construct_at(values.data() + 1, 17);
        std::construct_at(values.data() + 2, 99);
        EXPECT_EQ(values[1].value, 17);
        EXPECT_EQ(values[2].value, 99);
    }
    EXPECT_EQ(resource.outstanding, 0);
}

TEST(NativeCoreFrameArray, UninitialisedResizePreservesStateWhenAllocationFails) {
    TrackedValue::reset_counts();
    RecordingResource resource;
    {
        ml::FrameArray<TrackedValue> values{&resource};
        resource.fail_next = true;
        EXPECT_THROW(values.set_num_uninitialised(1), std::bad_alloc);
        EXPECT_TRUE(values.is_empty());
        EXPECT_EQ(values.data(), nullptr);
        EXPECT_EQ(resource.outstanding, 0);

        values.reserve(1);
        values.emplace(42);
        auto* const storage{values.data()};
        resource.fail_next = true;
        EXPECT_THROW(values.set_num_uninitialised(3), std::bad_alloc);
        EXPECT_EQ(values.num(), 1);
        EXPECT_EQ(values.data(), storage);
        EXPECT_EQ(values[0].value, 42);
        EXPECT_EQ(TrackedValue::live_count, 1);
        EXPECT_EQ(TrackedValue::move_count, 0);
        EXPECT_EQ(TrackedValue::destruction_count, 0);
        EXPECT_EQ(resource.outstanding, 1);

        values.set_num_uninitialised(3);
        std::construct_at(values.data() + 1, 17);
        std::construct_at(values.data() + 2, 99);
        EXPECT_EQ(values.num(), 3);
        EXPECT_EQ(values[0].value, 42);
        EXPECT_EQ(TrackedValue::live_count, 3);
    }
    EXPECT_EQ(TrackedValue::live_count, 0);
    EXPECT_EQ(resource.outstanding, 0);
}

TEST(NativeCoreFrameArray, ConsumesAliasedArgumentsBeforeGrowingStorage) {
    RecordingResource resource;
    {
        ml::FrameArray<TrackedValue> values{&resource};
        values.reserve(1);
        values.emplace(42);
        values.add(values[0]);
        EXPECT_EQ(values[0].value, 42);
        EXPECT_EQ(values[1].value, 42);
        values.emplace(values[0].value);
        EXPECT_EQ(values[0].value, 42);
        EXPECT_EQ(values[2].value, 42);
    }
    EXPECT_EQ(TrackedValue::live_count, 0);

    {
        ml::FrameArray<MoveOnlyValue> values{&resource};
        values.reserve(1);
        values.emplace(17);
        values.add(std::move(values[0]));
        EXPECT_EQ(values[0].value, -1);
        EXPECT_EQ(values[1].value, 17);
        values.reserve(8);
        EXPECT_EQ(values[1].value, 17);
    }
    EXPECT_EQ(resource.outstanding, 0);
}

TEST(NativeCoreFrameArray, PreservesValuesWhenAllocationFails) {
    RecordingResource resource;
    {
        ml::FrameArray<MoveOnlyValue> values{&resource};
        resource.fail_next = true;
        EXPECT_THROW(values.emplace(9), std::bad_alloc);
        EXPECT_TRUE(values.is_empty());
        EXPECT_EQ(values.data(), nullptr);
        EXPECT_EQ(resource.outstanding, 0);

        values.emplace(42);
        auto* const storage{values.data()};
        resource.fail_next = true;
        EXPECT_THROW(values.reserve(8), std::bad_alloc);
        resource.fail_next = true;
        EXPECT_THROW(values.add(std::move(values[0])), std::bad_alloc);
        EXPECT_EQ(values.data(), storage);
        EXPECT_EQ(values.num(), 1);
        EXPECT_EQ(values[0].value, 42);
        EXPECT_EQ(resource.outstanding, 1);
    }
    EXPECT_EQ(resource.outstanding, 0);
}

TEST(NativeCoreFrameArray, RollsBackFailedInsertionAndPartialResize) {
    ASSERT_EQ(ThrowingValue::live_count, 0);
    RecordingResource resource;
    for (bool const reserve_spare : {false, true}) {
        ThrowingValue::constructions_before_throw = -1;
        {
            ml::FrameArray<ThrowingValue> values{&resource};
            values.reserve(reserve_spare ? 8 : 1);
            values.emplace(42);
            auto* const storage{values.data()};

            ThrowingValue::constructions_before_throw = 0;
            EXPECT_THROW(values.emplace(7), std::runtime_error);
            EXPECT_THROW(values.add(values[0]), std::runtime_error);
            ThrowingValue::constructions_before_throw = 1;
            EXPECT_THROW(values.set_num(4), std::runtime_error);
            EXPECT_EQ(values.data(), storage);
            EXPECT_EQ(values.num(), 1);
            EXPECT_EQ(values[0].value, 42);
            EXPECT_EQ(ThrowingValue::live_count, 1);
            EXPECT_EQ(resource.outstanding, 1);

            ThrowingValue::constructions_before_throw = -1;
            values.set_num(4);
            EXPECT_EQ(values[0].value, 42);
            EXPECT_EQ(values[3].value, 0);
        }
        EXPECT_EQ(ThrowingValue::live_count, 0);
        EXPECT_EQ(resource.outstanding, 0);
    }
}

TEST(NativeCoreFrameArray, ForwardsExplicitElementResourcesWithoutAllocatorInjection) {
    RecordingResource storage_resource;
    RecordingResource element_resource;
    {
        ml::FrameArray<std::pmr::string> values{&storage_resource};
        values.emplace(128, 'x', std::pmr::polymorphic_allocator<char>{&element_resource});
        values.reserve(4);
        EXPECT_EQ(values[0].size(), 128);
        EXPECT_EQ(values[0].get_allocator().resource(), &element_resource);
        EXPECT_EQ(storage_resource.outstanding, 1);
        EXPECT_EQ(element_resource.outstanding, 1);
    }
    EXPECT_EQ(storage_resource.outstanding, 0);
    EXPECT_EQ(element_resource.outstanding, 0);
}

TEST(NativeCoreFrameArray, BalancesFrameResourceAllocationsAfterGrowthAndFailure) {
    alignas(ml::FrameMemoryResource::backing_alignment) std::array<std::byte, 4096> backing{};
    ml::FrameMemoryResource resource{backing};
    ThrowingValue::constructions_before_throw = -1;
    {
        ml::FrameArray<ThrowingValue> values{&resource};
        values.emplace(42);
        values.reserve(2);
        values.emplace(17);
        ThrowingValue::constructions_before_throw = 1;
        EXPECT_THROW(values.set_num(5), std::runtime_error);
        EXPECT_EQ(resource.get_stats().outstanding_allocation_count, 1);
        ThrowingValue::constructions_before_throw = -1;
        values.emplace(99);
        EXPECT_EQ(values[0].value, 42);
    }
    EXPECT_EQ(ThrowingValue::live_count, 0);
    EXPECT_EQ(resource.get_stats().outstanding_allocation_count, 0);
    EXPECT_TRUE(resource.try_reclaim());
}
}
