#include "Sandbox/containers/LockFreeMPSCQueue.h"

#include "Containers/UnrealString.h"
#include "Misc/AutomationTest.h"

template <typename T>
void run_queue_test(FAutomationTestBase* test, TArray<TArray<T>> const& batches) {
    ml::LockFreeMPSCQueue<T> queue{};
    auto const capacity{batches.Num() > 0 ? batches[0].Num() : 0};
    auto const init_result{queue.init(capacity)};
    test->TestEqual(
        TEXT("Queue initialization"), init_result, ml::ELockFreeMPSCQueueInitResult::Success);

    for (auto const& batch : batches) {
        for (auto const& value : batch) {
            auto const enqueue_result{queue.enqueue(value)};
            test->TestEqual(TEXT("Enqueue result"),
                            enqueue_result,
                            ml::ELockFreeMPSCQueueEnqueueResult::Success);
        }

        auto const result_span{queue.swap_and_consume()};
        test->TestEqual(
            TEXT("Result span size"), static_cast<int32>(result_span.size()), batch.Num());

        for (int32 i{0}; i < batch.Num(); ++i) {
            test->TestEqual(TEXT("Value at index"), result_span[i], batch[i]);
        }
    }
}

BEGIN_DEFINE_SPEC(FLockFreeMPSCQueueInt32Spec,
                  "Sandbox.LockFreeMPSCQueue.Int32",
                  EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
TArray<TTuple<FString, TArray<TArray<int32>>>> test_cases;
END_DEFINE_SPEC(FLockFreeMPSCQueueInt32Spec)

void FLockFreeMPSCQueueInt32Spec::Define() {
    test_cases = {
        MakeTuple(FString("Single batch with multiple values"),
                  TArray<TArray<int32>>{{1, 2, 3, 4, 5}}),
        MakeTuple(FString("Multiple batches"),
                  TArray<TArray<int32>>{{10, 20, 30}, {40, 50, 60}, {70, 80, 90}}),
        MakeTuple(FString("Empty and non-empty batches"),
                  TArray<TArray<int32>>{{100, 200}, {}, {300, 400}}),
    };

    Describe("LockFreeMPSCQueue<int32> - Basic operations", [this]() {
        for (auto const& test_case : test_cases) {
            auto const& name{test_case.Get<0>()};
            auto const& batches{test_case.Get<1>()};

            It(*name, [this, batches]() { run_queue_test(this, batches); });
        }
    });

    Describe("swap_and_consume edge cases", [this]() {
        It("should return empty span when called on empty queue", [this]() {
            ml::LockFreeMPSCQueue<int32> queue{};
            (void)queue.init(10);

            auto const result{queue.swap_and_consume()};
            TestEqual(TEXT("Empty queue span size"), static_cast<int32>(result.size()), 0);
        });

        It("should return empty span when called twice in a row", [this]() {
            ml::LockFreeMPSCQueue<int32> queue{};
            (void)queue.init(10);

            (void)queue.enqueue(42);
            auto const first_result{queue.swap_and_consume()};
            TestEqual(TEXT("First call span size"), static_cast<int32>(first_result.size()), 1);

            auto const second_result{queue.swap_and_consume()};
            TestEqual(TEXT("Second call span size"), static_cast<int32>(second_result.size()), 0);
        });
    });

    Describe("swap_and_visit", [this]() {
        It("should invoke callable with correct span", [this]() {
            ml::LockFreeMPSCQueue<int32> queue{};
            (void)queue.init(5);

            (void)queue.enqueue(10);
            (void)queue.enqueue(20);
            (void)queue.enqueue(30);

            int32 sum{0};
            queue.swap_and_visit([&sum](std::span<int32> span) {
                for (auto const value : span) {
                    sum += value;
                }
            });

            TestEqual(TEXT("Sum of values"), sum, 60);
        });

        It("should work with empty queue", [this]() {
            ml::LockFreeMPSCQueue<int32> queue{};
            (void)queue.init(5);

            bool called{false};
            (void)queue.swap_and_visit([&called](std::span<int32> span) {
                called = true;
                return span.size();
            });

            TestTrue(TEXT("Callable was invoked"), called);
        });
    });

    Describe("Initialization edge cases", [this]() {
        It("should succeed when initialized with capacity 0", [this]() {
            ml::LockFreeMPSCQueue<int32> queue{};
            auto const result{queue.init(0)};
            TestEqual(
                TEXT("Init with 0 capacity"), result, ml::ELockFreeMPSCQueueInitResult::Success);
        });

        It("should return AlreadyInitialised when init called twice", [this]() {
            ml::LockFreeMPSCQueue<int32> queue{};
            (void)queue.init(10);
            auto const result{queue.init(10)};
            TestEqual(TEXT("Second init call"),
                      result,
                      ml::ELockFreeMPSCQueueInitResult::AlreadyInitialised);
        });
    });

    Describe("Enqueue without initialization", [this]() {
        It("should return Uninitialised when enqueuing to uninitialized queue", [this]() {
            ml::LockFreeMPSCQueue<int32> queue{};
            auto const result{queue.enqueue(42)};
            TestEqual(TEXT("Enqueue on uninitialised"),
                      result,
                      ml::ELockFreeMPSCQueueEnqueueResult::Uninitialised);
        });
    });

    Describe("Queue full behavior", [this]() {
        It("should return Full when queue is at capacity", [this]() {
            ml::LockFreeMPSCQueue<int32> queue{};
            (void)queue.init(3);

            TestEqual(TEXT("First enqueue"),
                      queue.enqueue(1),
                      ml::ELockFreeMPSCQueueEnqueueResult::Success);
            TestEqual(TEXT("Second enqueue"),
                      queue.enqueue(2),
                      ml::ELockFreeMPSCQueueEnqueueResult::Success);
            TestEqual(TEXT("Third enqueue"),
                      queue.enqueue(3),
                      ml::ELockFreeMPSCQueueEnqueueResult::Success);
            TestEqual(TEXT("Fourth enqueue"),
                      queue.enqueue(4),
                      ml::ELockFreeMPSCQueueEnqueueResult::Full);
        });

        It("should allow enqueue after swap_and_consume", [this]() {
            ml::LockFreeMPSCQueue<int32> queue{};
            (void)queue.init(2);

            (void)queue.enqueue(1);
            (void)queue.enqueue(2);
            TestEqual(
                TEXT("Queue full"), queue.enqueue(3), ml::ELockFreeMPSCQueueEnqueueResult::Full);

            (void)queue.swap_and_consume();

            TestEqual(TEXT("Enqueue after swap"),
                      queue.enqueue(4),
                      ml::ELockFreeMPSCQueueEnqueueResult::Success);
        });
    });

    Describe("try_emplace", [this]() {
        It("should construct values in place", [this]() {
            ml::LockFreeMPSCQueue<int32> queue{};
            (void)queue.init(3);

            TestEqual(TEXT("First try_emplace"),
                      queue.try_emplace(100),
                      ml::ELockFreeMPSCQueueEnqueueResult::Success);
            TestEqual(TEXT("Second try_emplace"),
                      queue.try_emplace(200),
                      ml::ELockFreeMPSCQueueEnqueueResult::Success);

            auto const result{queue.swap_and_consume()};
            TestEqual(TEXT("Result size"), static_cast<int32>(result.size()), 2);
            TestEqual(TEXT("First value"), result[0], 100);
            TestEqual(TEXT("Second value"), result[1], 200);
        });

        It("should return Full when queue is at capacity", [this]() {
            ml::LockFreeMPSCQueue<int32> queue{};
            (void)queue.init(2);

            (void)queue.try_emplace(1);
            (void)queue.try_emplace(2);
            TestEqual(TEXT("try_emplace when full"),
                      queue.try_emplace(3),
                      ml::ELockFreeMPSCQueueEnqueueResult::Full);
        });
    });

    Describe("Capacity queries", [this]() {
        It("should return correct capacity values", [this]() {
            ml::LockFreeMPSCQueue<int32> queue{};
            TestFalse(TEXT("Initially not initialized"), queue.is_initialised());
            TestEqual(
                TEXT("Initial buffer_capacity"), static_cast<int32>(queue.buffer_capacity()), 0);
            TestEqual(TEXT("Initial full_capacity"), static_cast<int32>(queue.full_capacity()), 0);

            (void)queue.init(10);
            TestTrue(TEXT("After init is_initialised"), queue.is_initialised());
            TestEqual(TEXT("After init buffer_capacity"),
                      static_cast<int32>(queue.buffer_capacity()),
                      10);
            TestEqual(
                TEXT("After init full_capacity"), static_cast<int32>(queue.full_capacity()), 20);
        });
    });

    Describe("Order preservation", [this]() {
        It("should maintain FIFO order", [this]() {
            ml::LockFreeMPSCQueue<int32> queue{};
            (void)queue.init(10);

            for (int32 i{0}; i < 10; ++i) {
                (void)queue.enqueue(i);
            }

            auto const result{queue.swap_and_consume()};
            for (int32 i{0}; i < 10; ++i) {
                TestEqual(*FString::Printf(TEXT("Value at index %d"), i), result[i], i);
            }
        });
    });
}

BEGIN_DEFINE_SPEC(FLockFreeMPSCQueueFStringSpec,
                  "Sandbox.LockFreeMPSCQueue.FString",
                  EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
TArray<TTuple<FString, TArray<TArray<FString>>>> test_cases;
END_DEFINE_SPEC(FLockFreeMPSCQueueFStringSpec)

void FLockFreeMPSCQueueFStringSpec::Define() {
    test_cases = {
        MakeTuple(FString("Single batch with multiple strings"),
                  TArray<TArray<FString>>{{TEXT("Hello"), TEXT("World"), TEXT("Test")}}),
        MakeTuple(FString("Multiple batches"),
                  TArray<TArray<FString>>{{TEXT("Batch"), TEXT("One"), TEXT("Data")},
                                          {TEXT("Batch"), TEXT("Two"), TEXT("Data")},
                                          {TEXT("Batch"), TEXT("Three"), TEXT("Data")}}),
        MakeTuple(FString("Empty and non-empty batches"),
                  TArray<TArray<FString>>{
                      {TEXT("First"), TEXT("Batch")}, {}, {TEXT("Third"), TEXT("Batch")}}),
    };

    Describe("LockFreeMPSCQueue<FString> - Basic operations", [this]() {
        for (auto const& test_case : test_cases) {
            auto const& name{test_case.Get<0>()};
            auto const& batches{test_case.Get<1>()};

            It(*name, [this, batches]() { run_queue_test(this, batches); });
        }
    });

    Describe("swap_and_visit with FString", [this]() {
        It("should invoke callable with correct span", [this]() {
            ml::LockFreeMPSCQueue<FString> queue{};
            (void)queue.init(3);

            (void)queue.enqueue(TEXT("Hello"));
            (void)queue.enqueue(TEXT("World"));

            FString concatenated{};
            queue.swap_and_visit([&concatenated](std::span<FString> span) {
                for (auto const& str : span) {
                    concatenated += str;
                }
            });

            TestEqual(TEXT("Concatenated string"), concatenated, TEXT("HelloWorld"));
        });
    });

    Describe("try_emplace with FString", [this]() {
        It("should forward construct FString values", [this]() {
            ml::LockFreeMPSCQueue<FString> queue{};
            (void)queue.init(3);

            (void)queue.try_emplace(TEXT("Forward"));
            (void)queue.try_emplace(TEXT("Constructed"));

            auto const result{queue.swap_and_consume()};
            TestEqual(TEXT("Result size"), static_cast<int32>(result.size()), 2);
            TestEqual(TEXT("First value"), result[0], TEXT("Forward"));
            TestEqual(TEXT("Second value"), result[1], TEXT("Constructed"));
        });
    });

    Describe("Order preservation with FString", [this]() {
        It("should maintain FIFO order", [this]() {
            ml::LockFreeMPSCQueue<FString> queue{};
            (void)queue.init(5);

            TArray<FString> expected{
                TEXT("First"), TEXT("Second"), TEXT("Third"), TEXT("Fourth"), TEXT("Fifth")};
            for (auto const& str : expected) {
                (void)queue.enqueue(str);
            }

            auto const result{queue.swap_and_consume()};
            for (int32 i{0}; i < expected.Num(); ++i) {
                TestEqual(*FString::Printf(TEXT("String at index %d"), i), result[i], expected[i]);
            }
        });
    });
}
