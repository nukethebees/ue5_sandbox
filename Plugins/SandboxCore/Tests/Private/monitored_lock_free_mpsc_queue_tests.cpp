#include "SandboxCore/lock_free_mpsc_queue.h"
#include "SandboxCore/monitored_lock_free_mpsc_queue.h"

#include "TestHarness.h"

#include <cstdint>

TEST_CASE("SandboxCore.MonitoredLockFreeMPSCQueue.Counts enqueue results") {
    using FQueue = ml::MonitoredLockFreeMPSCQueue<ml::LockFreeMPSCQueue<std::int32_t>>;

    FQueue queue{};
    CHECK(queue.enqueue(0) == ml::ELockFreeMPSCQueueEnqueueResult::Uninitialised);
    REQUIRE(queue.init(2) == ml::ELockFreeMPSCQueueInitResult::Success);
    CHECK(queue.enqueue(10) == ml::ELockFreeMPSCQueueEnqueueResult::Success);
    CHECK(queue.enqueue(20) == ml::ELockFreeMPSCQueueEnqueueResult::Success);
    CHECK(queue.enqueue(30) == ml::ELockFreeMPSCQueueEnqueueResult::Full);

    CHECK(queue.get_success_count() == 2);
    CHECK(queue.get_full_count() == 1);
    CHECK(queue.get_uninitialised_count() == 1);

    auto const result{queue.swap_and_consume()};
    REQUIRE(result.view.size() == 2);
    CHECK(result.view[0] == 10);
    CHECK(result.view[1] == 20);
    CHECK(result.success_count == 2);
    CHECK(result.full_count == 1);
    CHECK(result.uninitialised_count == 1);
    CHECK_FALSE(result.is_empty());

    CHECK(queue.get_success_count() == 0);
    CHECK(queue.get_full_count() == 0);
    CHECK(queue.get_uninitialised_count() == 0);
}

TEST_CASE("SandboxCore.MonitoredLockFreeMPSCQueue.Resets counters") {
    using FQueue = ml::MonitoredLockFreeMPSCQueue<ml::LockFreeMPSCQueue<std::int32_t>>;

    FQueue queue{};
    REQUIRE(queue.init(1) == ml::ELockFreeMPSCQueueInitResult::Success);
    REQUIRE(queue.enqueue(10) == ml::ELockFreeMPSCQueueEnqueueResult::Success);
    REQUIRE(queue.enqueue(20) == ml::ELockFreeMPSCQueueEnqueueResult::Full);

    queue.reset_counters();

    CHECK(queue.get_success_count() == 0);
    CHECK(queue.get_full_count() == 0);
    CHECK(queue.get_uninitialised_count() == 0);

    auto const result{queue.swap_and_consume()};
    CHECK(result.success_count == 0);
    CHECK(result.full_count == 0);
    CHECK(result.uninitialised_count == 0);
    CHECK(result.is_empty());
}
