#include <sandbox/core/lock_free_mpsc_queue.h>
#include <sandbox/core/lock_free_mpsc_queue_soa.h>

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <thread>
#include <tuple>
#include <vector>

namespace {
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
}
