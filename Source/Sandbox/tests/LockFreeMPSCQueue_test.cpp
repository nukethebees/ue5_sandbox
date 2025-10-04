#include "CoreMinimal.h"
// #include "Misc/AutomationTest.h"

#include "Sandbox/containers/LockFreeMPSCQueue.h"

template <typename T>
void run_queue_test(FAutomationTestBase* test,
                    ml::LockFreeMPSCQueue<T>& queue,
                    TArray<TArray<T>> const& batches) {
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

    Describe("LockFreeMPSCQueue<int32>", [this]() {
        for (auto const& test_case : test_cases) {
            auto const& name{test_case.Get<0>()};
            auto const& batches{test_case.Get<1>()};

            It(*name, [this, batches]() {
                ml::LockFreeMPSCQueue<int32> queue{};
                run_queue_test(this, queue, batches);
            });
        }
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

    Describe("LockFreeMPSCQueue<FString>", [this]() {
        for (auto const& test_case : test_cases) {
            auto const& name{test_case.Get<0>()};
            auto const& batches{test_case.Get<1>()};

            It(*name, [this, batches]() {
                ml::LockFreeMPSCQueue<FString> queue{};
                run_queue_test(this, queue, batches);
            });
        }
    });
}
