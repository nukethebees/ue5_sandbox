#include "ioj/lock_free_mpsc_queue.h"
#include "ioj/monitored_lock_free_mpsc_queue.h"

#include <gtest/gtest.h>

#include <cstdint>

TEST(MonitoredLockFreeMPSCQueue, CountsAndConsumesEnqueueResults) {
    using Queue = ioj::MonitoredLockFreeMPSCQueue<ioj::LockFreeMPSCQueue<std::int32_t>>;

    Queue queue{};
    EXPECT_EQ(queue.enqueue(0), ioj::ELockFreeMPSCQueueEnqueueResult::Uninitialised);
    ASSERT_EQ(queue.init(2), ioj::ELockFreeMPSCQueueInitResult::Success);
    EXPECT_EQ(queue.enqueue(10), ioj::ELockFreeMPSCQueueEnqueueResult::Success);
    EXPECT_EQ(queue.enqueue(20), ioj::ELockFreeMPSCQueueEnqueueResult::Success);
    EXPECT_EQ(queue.enqueue(30), ioj::ELockFreeMPSCQueueEnqueueResult::Full);

    auto const result{queue.swap_and_consume()};
    ASSERT_EQ(result.view.size(), 2);
    EXPECT_EQ(result.view[0], 10);
    EXPECT_EQ(result.view[1], 20);
    EXPECT_EQ(result.success_count, 2);
    EXPECT_EQ(result.full_count, 1);
    EXPECT_EQ(result.uninitialised_count, 1);
    EXPECT_FALSE(result.is_empty());
    EXPECT_EQ(queue.get_success_count(), 0);
}

TEST(MonitoredLockFreeMPSCQueue, ResetsCounters) {
    using Queue = ioj::MonitoredLockFreeMPSCQueue<ioj::LockFreeMPSCQueue<std::int32_t>>;

    Queue queue{};
    ASSERT_EQ(queue.init(1), ioj::ELockFreeMPSCQueueInitResult::Success);
    ASSERT_EQ(queue.enqueue(10), ioj::ELockFreeMPSCQueueEnqueueResult::Success);
    ASSERT_EQ(queue.enqueue(20), ioj::ELockFreeMPSCQueueEnqueueResult::Full);

    queue.reset_counters();
    auto const result{queue.swap_and_consume()};
    EXPECT_TRUE(result.is_empty());
    EXPECT_EQ(result.success_count, 0);
    EXPECT_EQ(result.full_count, 0);
    EXPECT_EQ(result.uninitialised_count, 0);
}
