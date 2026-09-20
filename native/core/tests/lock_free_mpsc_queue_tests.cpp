#include <sandbox/core/lock_free_mpsc_queue.h>
#include <sandbox/core/lock_free_mpsc_queue_soa.h>

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <span>
#include <thread>
#include <tuple>
#include <vector>

namespace {
struct QueueView {
    std::span<std::int32_t const> ids;
    std::span<float const> weights;
};

struct TrackedValue {
    explicit TrackedValue(std::int32_t const value)
        : value{value} {
        ++live_count;
    }

    TrackedValue(TrackedValue&& other) noexcept
        : value{other.value} {
        ++live_count;
        other.value = -1;
    }

    ~TrackedValue() { --live_count; }

    TrackedValue(TrackedValue const&) = delete;
    auto operator=(TrackedValue const&) -> TrackedValue& = delete;
    auto operator=(TrackedValue&&) -> TrackedValue& = delete;

    std::int32_t value{};
    inline static std::int32_t live_count{};
};

struct alignas(64) OverAlignedValue {
    explicit OverAlignedValue(std::int32_t const value) noexcept
        : value{value} {}

    std::int32_t value{};
};

TEST(NativeCoreLockFreeMpscQueue, ReportsCapacityAndReusesBuffers) {
    ml::LockFreeMPSCQueue<std::int32_t> queue;

    EXPECT_EQ(queue.enqueue(1), ml::ELockFreeMPSCQueueEnqueueResult::Uninitialised);
    EXPECT_EQ(queue.init(2), ml::ELockFreeMPSCQueueInitResult::Success);
    EXPECT_EQ(queue.enqueue(10), ml::ELockFreeMPSCQueueEnqueueResult::Success);
    EXPECT_EQ(queue.enqueue(20), ml::ELockFreeMPSCQueueEnqueueResult::Success);
    EXPECT_EQ(queue.enqueue(30), ml::ELockFreeMPSCQueueEnqueueResult::Full);

    auto first{queue.swap_and_consume()};
    ASSERT_EQ(first.size(), 2u);
    EXPECT_EQ(first[0], 10);
    EXPECT_EQ(first[1], 20);

    EXPECT_EQ(queue.enqueue(30), ml::ELockFreeMPSCQueueEnqueueResult::Success);
    auto second{queue.swap_and_consume()};
    ASSERT_EQ(second.size(), 1u);
    EXPECT_EQ(second[0], 30);
}

TEST(NativeCoreLockFreeMpscQueue, AcceptsMultipleConcurrentProducers) {
    constexpr std::int32_t producer_count{4};
    constexpr std::int32_t values_per_producer{128};
    ml::LockFreeMPSCQueue<std::int32_t> queue;
    ASSERT_EQ(queue.init(producer_count * values_per_producer),
              ml::ELockFreeMPSCQueueInitResult::Success);

    std::vector<std::thread> producers;
    for (std::int32_t producer{}; producer < producer_count; ++producer) {
        producers.emplace_back([&, producer] {
            for (std::int32_t value{}; value < values_per_producer; ++value) {
                auto const encoded{producer * values_per_producer + value};
                EXPECT_EQ(queue.enqueue(encoded), ml::ELockFreeMPSCQueueEnqueueResult::Success);
            }
        });
    }
    for (auto& producer : producers) {
        producer.join();
    }

    auto values{queue.swap_and_consume()};
    EXPECT_EQ(values.size(), static_cast<std::size_t>(producer_count * values_per_producer));
}

TEST(NativeCoreLockFreeMpscQueueSoa, StoresColumnsAndPreservesRowOrder) {
    ml::LockFreeMPSCQueueSoA<void, std::int32_t, float> queue;
    ASSERT_EQ(queue.init(2), ml::ELockFreeMPSCQueueInitResult::Success);
    EXPECT_EQ(queue.enqueue(7, 1.5f), ml::ELockFreeMPSCQueueEnqueueResult::Success);
    EXPECT_EQ(queue.enqueue(9, 2.5f), ml::ELockFreeMPSCQueueEnqueueResult::Success);

    auto [ids, weights]{queue.swap_and_consume()};
    ASSERT_EQ(ids.size(), 2u);
    EXPECT_EQ(ids[0], 7);
    EXPECT_FLOAT_EQ(weights[0], 1.5f);
    EXPECT_EQ(ids[1], 9);
    EXPECT_FLOAT_EQ(weights[1], 2.5f);
}

TEST(NativeCoreLockFreeMpscQueue, SupportsZeroCapacityAndDestroysRotatedBuffers) {
    ml::LockFreeMPSCQueue<std::int32_t> empty;
    EXPECT_EQ(empty.init(0), ml::ELockFreeMPSCQueueInitResult::Success);
    EXPECT_FALSE(empty.is_initialised());

    EXPECT_EQ(TrackedValue::live_count, 0);
    {
        ml::LockFreeMPSCQueue<TrackedValue> queue;
        ASSERT_EQ(queue.init(2), ml::ELockFreeMPSCQueueInitResult::Success);
        ASSERT_EQ(queue.enqueue(10), ml::ELockFreeMPSCQueueEnqueueResult::Success);
        ASSERT_EQ(queue.enqueue(20), ml::ELockFreeMPSCQueueEnqueueResult::Success);
        EXPECT_EQ(queue.enqueue(30), ml::ELockFreeMPSCQueueEnqueueResult::Full);

        auto const first{queue.swap_and_consume()};
        ASSERT_EQ(first.size(), 2u);
        ASSERT_EQ(queue.enqueue(30), ml::ELockFreeMPSCQueueEnqueueResult::Success);
        auto const second{queue.swap_and_consume()};
        ASSERT_EQ(second.size(), 1u);
        EXPECT_EQ(second[0].value, 30);
    }
    EXPECT_EQ(TrackedValue::live_count, 0);
}

TEST(NativeCoreLockFreeMpscQueueSoa, SupportsCustomViewsAndOverAlignedColumns) {
    ml::LockFreeMPSCQueueSoA<QueueView, std::int32_t, float> queue;
    ASSERT_EQ(queue.init(2), ml::ELockFreeMPSCQueueInitResult::Success);
    ASSERT_EQ(queue.enqueue(10, 1.5f), ml::ELockFreeMPSCQueueEnqueueResult::Success);
    ASSERT_EQ(queue.enqueue(20, 2.5f), ml::ELockFreeMPSCQueueEnqueueResult::Success);

    EXPECT_FLOAT_EQ(queue.swap_and_visit([](QueueView const view) {
        return static_cast<float>(view.ids[0]) + view.weights[1];
    }),
                    12.5f);

    ml::LockFreeMPSCQueueSoA<void, std::uint8_t, OverAlignedValue> aligned_queue;
    ASSERT_EQ(aligned_queue.init(1), ml::ELockFreeMPSCQueueInitResult::Success);
    ASSERT_EQ(aligned_queue.enqueue(std::uint8_t{1}, 42),
              ml::ELockFreeMPSCQueueEnqueueResult::Success);
    auto const [bytes, values]{aligned_queue.swap_and_consume()};
    ASSERT_EQ(bytes.size(), 1u);
    ASSERT_EQ(values.size(), 1u);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(values.data()) % alignof(OverAlignedValue), 0u);
    EXPECT_EQ(values[0].value, 42);
}
}
