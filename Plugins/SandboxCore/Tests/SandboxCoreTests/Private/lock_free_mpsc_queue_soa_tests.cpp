#include "SandboxCore/lock_free_mpsc_queue_soa.h"

#include "CoreMinimal.h"
#include "TestHarness.h"

#include <algorithm>
#include <atomic>
#include <barrier>
#include <cstdint>
#include <span>
#include <thread>
#include <utility>
#include <vector>

namespace {
struct FIntFloatQueueView {
    std::span<std::int32_t const> integers;
    std::span<float const> floats;
};

struct FIntVectorQueueView {
    std::span<std::int32_t const> integers;
    std::span<FVector const> vectors;
};

template <std::int32_t Tag>
struct TLifetimeTrackedValue {
    explicit TLifetimeTrackedValue(std::int32_t const in_value) noexcept
        : value{in_value} {
        ++live_count;
    }

    TLifetimeTrackedValue(TLifetimeTrackedValue&& other) noexcept
        : value{other.value} {
        ++live_count;
        other.value = -1;
    }

    ~TLifetimeTrackedValue() { --live_count; }

    std::int32_t value{};
    inline static std::int32_t live_count{};
};

using FFirstLifetimeValue = TLifetimeTrackedValue<0>;
using FSecondLifetimeValue = TLifetimeTrackedValue<1>;

struct alignas(64) FOverAlignedQueueValue {
    explicit FOverAlignedQueueValue(std::int32_t const in_value) noexcept
        : value{in_value} {}

    std::int32_t value{};
};
}

TEST_CASE("SandboxCore.LockFreeMPSCQueueSoA.Default queue is uninitialised") {
    ml::LockFreeMPSCQueueSoA<void, std::int32_t, float> queue{};

    CHECK_FALSE(queue.is_initialised());
    CHECK(queue.buffer_capacity() == 0);
    CHECK(queue.full_capacity() == 0);
    CHECK(queue.enqueue(1, 1.0f) == ml::ELockFreeMPSCQueueEnqueueResult::Uninitialised);

    auto const [integers, floats]{queue.swap_and_consume()};
    CHECK(integers.empty());
    CHECK(floats.empty());
}

TEST_CASE("SandboxCore.LockFreeMPSCQueueSoA.Initialisation and capacity results") {
    ml::LockFreeMPSCQueueSoA<void, std::int32_t, float> zero_capacity_queue{};
    CHECK(zero_capacity_queue.init(0) == ml::ELockFreeMPSCQueueInitResult::Success);
    CHECK_FALSE(zero_capacity_queue.is_initialised());

    ml::LockFreeMPSCQueueSoA<void, std::int32_t, float> queue{};
    REQUIRE(queue.init(2) == ml::ELockFreeMPSCQueueInitResult::Success);
    CHECK(queue.is_initialised());
    CHECK(queue.buffer_capacity() == 2);
    CHECK(queue.full_capacity() == 4);
    CHECK(queue.init(2) == ml::ELockFreeMPSCQueueInitResult::AlreadyInitialised);
}

TEST_CASE("SandboxCore.LockFreeMPSCQueueSoA.Consumes FIFO batches and reuses capacity") {
    ml::LockFreeMPSCQueueSoA<void, std::int32_t, float, FVector> queue{};
    REQUIRE(queue.init(2) == ml::ELockFreeMPSCQueueInitResult::Success);

    CHECK(queue.enqueue(10, 1.5f, FVector{1.0, 2.0, 3.0}) == ml::ELockFreeMPSCQueueEnqueueResult::Success);
    CHECK(queue.enqueue(20, 2.5f, FVector{4.0, 5.0, 6.0}) == ml::ELockFreeMPSCQueueEnqueueResult::Success);
    CHECK(queue.enqueue(30, 3.5f, FVector::ZeroVector) == ml::ELockFreeMPSCQueueEnqueueResult::Full);

    auto const [integers, floats, vectors]{queue.swap_and_consume()};
    REQUIRE(integers.size() == 2);
    REQUIRE(floats.size() == 2);
    REQUIRE(vectors.size() == 2);
    CHECK(integers[0] == 10);
    CHECK(integers[1] == 20);
    CHECK(floats[0] == 1.5f);
    CHECK(floats[1] == 2.5f);
    CHECK(vectors[0] == FVector(1.0, 2.0, 3.0));
    CHECK(vectors[1] == FVector(4.0, 5.0, 6.0));

    CHECK(queue.enqueue(30, 3.5f, FVector::ZeroVector) == ml::ELockFreeMPSCQueueEnqueueResult::Success);
    auto const [next_integers, next_floats, next_vectors]{queue.swap_and_consume()};
    REQUIRE(next_integers.size() == 1);
    CHECK(next_integers[0] == 30);
    CHECK(next_floats[0] == 3.5f);
    CHECK(next_vectors[0] == FVector::ZeroVector);
}

TEST_CASE("SandboxCore.LockFreeMPSCQueueSoA.Supports custom views and SwapAndVisit") {
    ml::LockFreeMPSCQueueSoA<FIntFloatQueueView, std::int32_t, float> queue{};
    REQUIRE(queue.init(3) == ml::ELockFreeMPSCQueueInitResult::Success);
    REQUIRE(queue.enqueue(10, 1.5f) == ml::ELockFreeMPSCQueueEnqueueResult::Success);
    REQUIRE(queue.enqueue(20, 2.5f) == ml::ELockFreeMPSCQueueEnqueueResult::Success);

    auto const sum{queue.swap_and_visit([](FIntFloatQueueView const& view) {
        float result{0.0f};
        for (std::size_t i{0}; i < view.integers.size(); ++i) {
            result += static_cast<float>(view.integers[i]) + view.floats[i];
        }
        return result;
    })};
    CHECK(sum == 34.0f);

    auto const empty_view{queue.swap_and_consume()};
    CHECK(empty_view.integers.empty());
    CHECK(empty_view.floats.empty());
}

TEST_CASE("SandboxCore.LockFreeMPSCQueueSoA.Custom views support Unreal value types") {
    ml::LockFreeMPSCQueueSoA<FIntVectorQueueView, std::int32_t, FVector> queue{};
    REQUIRE(queue.init(2) == ml::ELockFreeMPSCQueueInitResult::Success);
    REQUIRE(queue.enqueue(5, FVector{1.0, 2.0, 3.0}) == ml::ELockFreeMPSCQueueEnqueueResult::Success);
    REQUIRE(queue.enqueue(10, FVector{4.0, 5.0, 6.0}) == ml::ELockFreeMPSCQueueEnqueueResult::Success);

    auto const view{queue.swap_and_consume()};
    REQUIRE(view.integers.size() == 2);
    REQUIRE(view.vectors.size() == 2);
    CHECK(view.integers[0] == 5);
    CHECK(view.integers[1] == 10);
    CHECK(view.vectors[0] == FVector(1.0, 2.0, 3.0));
    CHECK(view.vectors[1] == FVector(4.0, 5.0, 6.0));
}

TEST_CASE("SandboxCore.LockFreeMPSCQueueSoA.Accepts multiple concurrent producers") {
    constexpr std::int32_t producer_count{4};
    constexpr std::int32_t values_per_producer{256};
    constexpr std::int32_t value_count{producer_count * values_per_producer};

    ml::LockFreeMPSCQueueSoA<void, std::int32_t, std::int32_t> queue{};
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
                if (queue.enqueue(value, value + value_count) != ml::ELockFreeMPSCQueueEnqueueResult::Success) {
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
    auto const [values, companion_values]{queue.swap_and_consume()};
    REQUIRE(values.size() == value_count);
    REQUIRE(companion_values.size() == value_count);

    std::vector<std::pair<std::int32_t, std::int32_t>> pairs{};
    pairs.reserve(value_count);
    for (std::size_t i{0}; i < values.size(); ++i) {
        pairs.emplace_back(values[i], companion_values[i]);
    }
    std::ranges::sort(pairs);

    for (std::int32_t i{0}; i < value_count; ++i) {
        CHECK(pairs[i].first == i);
        CHECK(pairs[i].second == i + value_count);
    }
}

TEST_CASE("SandboxCore.LockFreeMPSCQueueSoA.Destroys every column as buffers rotate") {
    CHECK(FFirstLifetimeValue::live_count == 0);
    CHECK(FSecondLifetimeValue::live_count == 0);

    {
        ml::LockFreeMPSCQueueSoA<void, FFirstLifetimeValue, FSecondLifetimeValue> queue{};
        REQUIRE(queue.init(2) == ml::ELockFreeMPSCQueueInitResult::Success);
        REQUIRE(queue.enqueue(10, 110) == ml::ELockFreeMPSCQueueEnqueueResult::Success);
        REQUIRE(queue.enqueue(20, 120) == ml::ELockFreeMPSCQueueEnqueueResult::Success);
        CHECK(queue.enqueue(30, 130) == ml::ELockFreeMPSCQueueEnqueueResult::Full);
        CHECK(FFirstLifetimeValue::live_count == 2);
        CHECK(FSecondLifetimeValue::live_count == 2);

        auto const [first_values, second_values]{queue.swap_and_consume()};
        REQUIRE(first_values.size() == 2);
        REQUIRE(second_values.size() == 2);
        CHECK(first_values[1].value == 20);
        CHECK(second_values[1].value == 120);

        REQUIRE(queue.enqueue(30, 130) == ml::ELockFreeMPSCQueueEnqueueResult::Success);
        CHECK(FFirstLifetimeValue::live_count == 3);
        CHECK(FSecondLifetimeValue::live_count == 3);

        auto const [next_first_values, next_second_values]{queue.swap_and_consume()};
        REQUIRE(next_first_values.size() == 1);
        REQUIRE(next_second_values.size() == 1);
        CHECK(next_first_values[0].value == 30);
        CHECK(next_second_values[0].value == 130);
        CHECK(FFirstLifetimeValue::live_count == 1);
        CHECK(FSecondLifetimeValue::live_count == 1);

        REQUIRE(queue.enqueue(40, 140) == ml::ELockFreeMPSCQueueEnqueueResult::Success);
    }

    CHECK(FFirstLifetimeValue::live_count == 0);
    CHECK(FSecondLifetimeValue::live_count == 0);
}

TEST_CASE("SandboxCore.LockFreeMPSCQueueSoA.Aligns every column") {
    ml::LockFreeMPSCQueueSoA<void, std::uint8_t, FOverAlignedQueueValue, std::uint16_t> queue{};
    REQUIRE(queue.init(3) == ml::ELockFreeMPSCQueueInitResult::Success);
    REQUIRE(queue.enqueue(std::uint8_t{1}, 10, std::uint16_t{100}) == ml::ELockFreeMPSCQueueEnqueueResult::Success);

    auto const [bytes, aligned_values, shorts]{queue.swap_and_consume()};
    REQUIRE(bytes.size() == 1);
    REQUIRE(aligned_values.size() == 1);
    REQUIRE(shorts.size() == 1);
    CHECK(reinterpret_cast<uintptr_t>(bytes.data()) % alignof(std::uint8_t) == 0);
    CHECK(reinterpret_cast<uintptr_t>(aligned_values.data()) % alignof(FOverAlignedQueueValue) == 0);
    CHECK(reinterpret_cast<uintptr_t>(shorts.data()) % alignof(std::uint16_t) == 0);
    CHECK(aligned_values[0].value == 10);
}
