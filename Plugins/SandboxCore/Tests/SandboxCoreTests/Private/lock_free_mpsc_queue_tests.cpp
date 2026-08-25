#include "SandboxCore/lock_free_mpsc_queue.h"

#include "TestHarness.h"

#include <algorithm>
#include <atomic>
#include <barrier>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

namespace {
struct FLifetimeTrackedQueueValue {
    explicit FLifetimeTrackedQueueValue(std::int32_t const in_value) noexcept
        : value{in_value} {
        ++live_count;
    }

    FLifetimeTrackedQueueValue(FLifetimeTrackedQueueValue&& other) noexcept
        : value{other.value} {
        ++live_count;
        other.value = -1;
    }

    ~FLifetimeTrackedQueueValue() { --live_count; }

    std::int32_t value{};
    inline static std::int32_t live_count{};
};
}

TEST_CASE("SandboxCore.LockFreeMPSCQueue.Default queue is uninitialised") {
    ml::LockFreeMPSCQueue<std::int32_t> queue{};

    CHECK_FALSE(queue.is_initialised());
    CHECK(queue.buffer_capacity() == 0);
    CHECK(queue.full_capacity() == 0);
    CHECK(queue.enqueue(1) == ml::ELockFreeMPSCQueueEnqueueResult::Uninitialised);
    CHECK(queue.swap_and_consume().empty());
}

TEST_CASE("SandboxCore.LockFreeMPSCQueue.Initialisation and capacity results") {
    ml::LockFreeMPSCQueue<std::int32_t> zero_capacity_queue{};
    CHECK(zero_capacity_queue.init(0) == ml::ELockFreeMPSCQueueInitResult::Success);
    CHECK_FALSE(zero_capacity_queue.is_initialised());

    ml::LockFreeMPSCQueue<std::int32_t> queue{};
    REQUIRE(queue.init(3) == ml::ELockFreeMPSCQueueInitResult::Success);

    CHECK(queue.is_initialised());
    CHECK(queue.buffer_capacity() == 3);
    CHECK(queue.full_capacity() == 6);
    CHECK(queue.init(3) == ml::ELockFreeMPSCQueueInitResult::AlreadyInitialised);
}

TEST_CASE("SandboxCore.LockFreeMPSCQueue.Consumes FIFO batches and reuses capacity") {
    ml::LockFreeMPSCQueue<std::int32_t> queue{};
    REQUIRE(queue.init(3) == ml::ELockFreeMPSCQueueInitResult::Success);

    CHECK(queue.enqueue(10) == ml::ELockFreeMPSCQueueEnqueueResult::Success);
    CHECK(queue.enqueue(20) == ml::ELockFreeMPSCQueueEnqueueResult::Success);
    CHECK(queue.enqueue(30) == ml::ELockFreeMPSCQueueEnqueueResult::Success);
    CHECK(queue.enqueue(40) == ml::ELockFreeMPSCQueueEnqueueResult::Full);

    auto const first_batch{queue.swap_and_consume()};
    REQUIRE(first_batch.size() == 3);
    CHECK(first_batch[0] == 10);
    CHECK(first_batch[1] == 20);
    CHECK(first_batch[2] == 30);

    CHECK(queue.enqueue(40) == ml::ELockFreeMPSCQueueEnqueueResult::Success);
    auto const second_batch{queue.swap_and_consume()};
    REQUIRE(second_batch.size() == 1);
    CHECK(second_batch[0] == 40);

    CHECK(queue.swap_and_consume().empty());
}

TEST_CASE("SandboxCore.LockFreeMPSCQueue.SwapAndVisit handles values and empty batches") {
    ml::LockFreeMPSCQueue<std::int32_t> queue{};
    REQUIRE(queue.init(3) == ml::ELockFreeMPSCQueueInitResult::Success);
    REQUIRE(queue.enqueue(10) == ml::ELockFreeMPSCQueueEnqueueResult::Success);
    REQUIRE(queue.enqueue(20) == ml::ELockFreeMPSCQueueEnqueueResult::Success);

    auto const sum{queue.swap_and_visit([](auto const values) {
        std::int32_t result{0};
        for (auto const value : values) {
            result += value;
        }
        return result;
    })};
    CHECK(sum == 30);

    auto const empty_size{queue.swap_and_visit([](auto const values) { return values.size(); })};
    CHECK(empty_size == 0);
}

TEST_CASE("SandboxCore.LockFreeMPSCQueue.Supports non-trivial movable values") {
    ml::LockFreeMPSCQueue<std::unique_ptr<std::int32_t>> queue{};
    REQUIRE(queue.init(2) == ml::ELockFreeMPSCQueueInitResult::Success);

    REQUIRE(queue.enqueue(std::make_unique<std::int32_t>(11)) == ml::ELockFreeMPSCQueueEnqueueResult::Success);
    REQUIRE(queue.enqueue(std::make_unique<std::int32_t>(22)) == ml::ELockFreeMPSCQueueEnqueueResult::Success);

    auto const values{queue.swap_and_consume()};
    REQUIRE(values.size() == 2);
    CHECK(*values[0] == 11);
    CHECK(*values[1] == 22);

    CHECK(queue.swap_and_consume().empty());
}

TEST_CASE("SandboxCore.LockFreeMPSCQueue.Accepts multiple concurrent producers") {
    constexpr std::int32_t producer_count{4};
    constexpr std::int32_t values_per_producer{256};
    constexpr std::int32_t value_count{producer_count * values_per_producer};

    ml::LockFreeMPSCQueue<std::int32_t> queue{};
    REQUIRE(queue.init(value_count) == ml::ELockFreeMPSCQueueInitResult::Success);

    std::barrier start_barrier{producer_count + 1};
    std::atomic_size_t enqueue_failures{0};
    std::vector<std::thread> producers{};
    producers.reserve(producer_count);

    for (std::int32_t producer_index{0}; producer_index < producer_count; ++producer_index) {
        producers.emplace_back([&queue, &start_barrier, &enqueue_failures, producer_index]() {
            start_barrier.arrive_and_wait();
            for (std::int32_t i{0}; i < values_per_producer; ++i) {
                auto const value{producer_index * values_per_producer + i};
                if (queue.enqueue(value) != ml::ELockFreeMPSCQueueEnqueueResult::Success) {
                    ++enqueue_failures;
                }
            }
        });
    }

    start_barrier.arrive_and_wait();
    for (auto& producer : producers) {
        producer.join();
    }

    CHECK(enqueue_failures.load() == 0);
    auto const values{queue.swap_and_consume()};
    REQUIRE(values.size() == value_count);

    std::vector<std::int32_t> sorted_values{values.begin(), values.end()};
    std::ranges::sort(sorted_values);
    for (std::int32_t i{0}; i < value_count; ++i) {
        CHECK(sorted_values[i] == i);
    }
}

TEST_CASE("SandboxCore.LockFreeMPSCQueue.Destroys values as buffers rotate") {
    CHECK(FLifetimeTrackedQueueValue::live_count == 0);

    {
        ml::LockFreeMPSCQueue<FLifetimeTrackedQueueValue> queue{};
        REQUIRE(queue.init(2) == ml::ELockFreeMPSCQueueInitResult::Success);
        REQUIRE(queue.enqueue(10) == ml::ELockFreeMPSCQueueEnqueueResult::Success);
        REQUIRE(queue.enqueue(20) == ml::ELockFreeMPSCQueueEnqueueResult::Success);
        CHECK(FLifetimeTrackedQueueValue::live_count == 2);

        auto const first_batch{queue.swap_and_consume()};
        REQUIRE(first_batch.size() == 2);
        CHECK(first_batch[0].value == 10);
        CHECK(first_batch[1].value == 20);

        REQUIRE(queue.enqueue(30) == ml::ELockFreeMPSCQueueEnqueueResult::Success);
        CHECK(FLifetimeTrackedQueueValue::live_count == 3);

        auto const second_batch{queue.swap_and_consume()};
        REQUIRE(second_batch.size() == 1);
        CHECK(second_batch[0].value == 30);
        CHECK(FLifetimeTrackedQueueValue::live_count == 1);

        REQUIRE(queue.enqueue(40) == ml::ELockFreeMPSCQueueEnqueueResult::Success);
        CHECK(FLifetimeTrackedQueueValue::live_count == 2);
    }

    CHECK(FLifetimeTrackedQueueValue::live_count == 0);
}

TEST_CASE("SandboxCore.LockFreeMPSCQueue.Full enqueue does not construct a value") {
    CHECK(FLifetimeTrackedQueueValue::live_count == 0);

    {
        ml::LockFreeMPSCQueue<FLifetimeTrackedQueueValue> queue{};
        REQUIRE(queue.init(1) == ml::ELockFreeMPSCQueueInitResult::Success);
        REQUIRE(queue.enqueue(10) == ml::ELockFreeMPSCQueueEnqueueResult::Success);
        CHECK(queue.enqueue(20) == ml::ELockFreeMPSCQueueEnqueueResult::Full);
        CHECK(FLifetimeTrackedQueueValue::live_count == 1);
    }

    CHECK(FLifetimeTrackedQueueValue::live_count == 0);
}
