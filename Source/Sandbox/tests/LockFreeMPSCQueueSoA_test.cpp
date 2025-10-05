#include "Sandbox/containers/LockFreeMPSCQueueSoA.h"

#include "Containers/UnrealString.h"
#include "Misc/AutomationTest.h"

#define TEST_SPAN_EQUAL(Message, Span, Expected) \
    TestEqual(TEXT(Message), static_cast<int32>((Span).size()), Expected)

template <typename... Ts>
struct SoATestBatch {
    using BatchType = std::tuple<TArray<Ts>...>;

    FString name;
    TArray<BatchType> batches;

    template <typename StringType>
    SoATestBatch(StringType&& n, TArray<BatchType> b)
        : name(std::forward<StringType>(n))
        , batches(std::move(b)) {}
};

// Tests a LockFreeMPSCQueueSoA by enqueuing batches of parallel arrays and validating results.
// Each batch is a tuple of arrays (SoA-style), where each array corresponds to one type in Ts.
// For each batch, elements at the same index across all arrays are enqueued together.
template <typename... Ts>
void run_soa_queue_test(FAutomationTestBase* test,
                        TArray<std::tuple<TArray<Ts>...>> const& batches) {
    constexpr auto indexes{std::index_sequence_for<Ts...>{}};

    ml::LockFreeMPSCQueueSoA<void, Ts...> queue{};
    auto const capacity{batches.Num() > 0 ? std::get<0>(batches[0]).Num() : 0};
    auto const init_result{queue.init(capacity)};
    test->TestEqual(
        TEXT("Queue initialization"), init_result, ml::ELockFreeMPSCQueueInitResult::Success);

    for (auto const& batch : batches) {
        auto const batch_size{std::get<0>(batch).Num()};

        for (int32 i{0}; i < batch_size; ++i) {
            auto const enqueue_result{[&]<std::size_t... Is>(std::index_sequence<Is...>) {
                return queue.enqueue(std::get<Is>(batch)[i]...);
            }(indexes)};

            test->TestEqual(TEXT("Enqueue result"),
                            enqueue_result,
                            ml::ELockFreeMPSCQueueEnqueueResult::Success);
        }

        auto const result_spans{queue.swap_and_consume()};

        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ((test->TestEqual(*FString::Printf(TEXT("Result span %llu size"), Is),
                              static_cast<int32>(std::get<Is>(result_spans).size()),
                              batch_size)),
             ...);
        }(indexes);

        for (int32 i{0}; i < batch_size; ++i) {
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                ((test->TestEqual(TEXT("Value at index"),
                                  std::get<Is>(result_spans)[i],
                                  std::get<Is>(batch)[i])),
                 ...);
            }(indexes);
        }
    }
}

BEGIN_DEFINE_SPEC(FLockFreeMPSCQueueSoAInt32FloatSpec,
                  "Sandbox.LockFreeMPSCQueueSoA.Int32Float",
                  EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
TArray<SoATestBatch<int32, float>> test_cases;
END_DEFINE_SPEC(FLockFreeMPSCQueueSoAInt32FloatSpec)

void FLockFreeMPSCQueueSoAInt32FloatSpec::Define() {
    test_cases = {
        {"Single batch with multiple values", {{{1, 2, 3, 4, 5}, {1.0f, 2.0f, 3.0f, 4.0f, 5.0f}}}},
        {"Multiple batches",
         {{{10, 20}, {1.5f, 2.5f}}, {{30, 40}, {3.5f, 4.5f}}, {{50, 60}, {5.5f, 6.5f}}}},
        {"Empty and non-empty batches",
         {{{100, 200}, {10.0f, 20.0f}}, {{}, {}}, {{300, 400}, {30.0f, 40.0f}}}},
    };

    Describe("LockFreeMPSCQueueSoA<void, int32, float> - Basic operations", [this]() {
        for (auto const& test_case : test_cases) {
            It(*test_case.name,
               [this, batches = test_case.batches]() { run_soa_queue_test(this, batches); });
        }
    });

    Describe("swap_and_consume edge cases", [this]() {
        It("should return empty spans when called on empty queue", [this]() {
            ml::LockFreeMPSCQueueSoA<void, int32, float> queue{};
            (void)queue.init(10);

            auto const [int_span, float_span]{queue.swap_and_consume()};
            TEST_SPAN_EQUAL("Empty int span size", int_span, 0);
            TEST_SPAN_EQUAL("Empty float span size", float_span, 0);
        });

        It("should return empty spans when called twice in a row", [this]() {
            ml::LockFreeMPSCQueueSoA<void, int32, float> queue{};
            (void)queue.init(10);

            (void)queue.enqueue(42, 3.14f);
            auto const [int_span1, float_span1]{queue.swap_and_consume()};
            TEST_SPAN_EQUAL("First call int span size", int_span1, 1);
            TEST_SPAN_EQUAL("First call float span size", float_span1, 1);

            auto const [int_span2, float_span2]{queue.swap_and_consume()};
            TEST_SPAN_EQUAL("Second call int span size", int_span2, 0);
            TEST_SPAN_EQUAL("Second call float span size", float_span2, 0);
        });
    });

    Describe("swap_and_visit", [this]() {
        It("should invoke callable with correct spans", [this]() {
            ml::LockFreeMPSCQueueSoA<void, int32, float> queue{};
            (void)queue.init(5);

            (void)queue.enqueue(10, 1.0f);
            (void)queue.enqueue(20, 2.0f);
            (void)queue.enqueue(30, 3.0f);

            int32 int_sum{0};
            float float_sum{0.0f};
            queue.swap_and_visit([&](auto const& spans) {
                auto const& [int_span, float_span]{spans};
                for (auto const value : int_span) {
                    int_sum += value;
                }
                for (auto const value : float_span) {
                    float_sum += value;
                }
            });

            TestEqual(TEXT("Sum of ints"), int_sum, 60);
            TestEqual(TEXT("Sum of floats"), float_sum, 6.0f);
        });

        It("should work with empty queue", [this]() {
            ml::LockFreeMPSCQueueSoA<void, int32, float> queue{};
            (void)queue.init(5);

            bool called{false};
            (void)queue.swap_and_visit([&called](auto const& spans) {
                called = true;
                auto const& [int_span, float_span]{spans};
                return int_span.size() + float_span.size();
            });

            TestTrue(TEXT("Callable was invoked"), called);
        });
    });

    Describe("Initialization edge cases", [this]() {
        It("should succeed when initialized with capacity 0", [this]() {
            ml::LockFreeMPSCQueueSoA<void, int32, float> queue{};
            auto const result{queue.init(0)};
            TestEqual(
                TEXT("Init with 0 capacity"), result, ml::ELockFreeMPSCQueueInitResult::Success);
        });

        It("should return AlreadyInitialised when init called twice", [this]() {
            ml::LockFreeMPSCQueueSoA<void, int32, float> queue{};
            (void)queue.init(10);
            auto const result{queue.init(10)};
            TestEqual(TEXT("Second init call"),
                      result,
                      ml::ELockFreeMPSCQueueInitResult::AlreadyInitialised);
        });
    });

    Describe("Enqueue without initialization", [this]() {
        It("should return Uninitialised when enqueuing to uninitialized queue", [this]() {
            ml::LockFreeMPSCQueueSoA<void, int32, float> queue{};
            auto const result{queue.enqueue(42, 3.14f)};
            TestEqual(TEXT("Enqueue on uninitialised"),
                      result,
                      ml::ELockFreeMPSCQueueEnqueueResult::Uninitialised);
        });
    });

    Describe("Queue full behavior", [this]() {
        It("should return Full when queue is at capacity", [this]() {
            ml::LockFreeMPSCQueueSoA<void, int32, float> queue{};
            (void)queue.init(3);

            using enum ml::ELockFreeMPSCQueueEnqueueResult;

            TestEqual(TEXT("First enqueue"), queue.enqueue(1, 1.0f), Success);
            TestEqual(TEXT("Second enqueue"), queue.enqueue(2, 2.0f), Success);
            TestEqual(TEXT("Third enqueue"), queue.enqueue(3, 3.0f), Success);
            TestEqual(TEXT("Fourth enqueue"), queue.enqueue(4, 4.0f), Full);
        });

        It("should allow enqueue after swap_and_consume", [this]() {
            ml::LockFreeMPSCQueueSoA<void, int32, float> queue{};
            (void)queue.init(2);

            (void)queue.enqueue(1, 1.0f);
            (void)queue.enqueue(2, 2.0f);
            TestEqual(TEXT("Queue full"),
                      queue.enqueue(3, 3.0f),
                      ml::ELockFreeMPSCQueueEnqueueResult::Full);

            (void)queue.swap_and_consume();

            TestEqual(TEXT("Enqueue after swap"),
                      queue.enqueue(4, 4.0f),
                      ml::ELockFreeMPSCQueueEnqueueResult::Success);
        });
    });

    Describe("Capacity queries", [this]() {
        It("should return correct capacity values", [this]() {
            ml::LockFreeMPSCQueueSoA<void, int32, float> queue{};
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
            ml::LockFreeMPSCQueueSoA<void, int32, float> queue{};
            (void)queue.init(10);

            for (int32 i{0}; i < 10; ++i) {
                (void)queue.enqueue(i, static_cast<float>(i) * 10.0f);
            }

            auto const [int_span, float_span]{queue.swap_and_consume()};
            for (int32 i{0}; i < 10; ++i) {
                TestEqual(*FString::Printf(TEXT("Int value at index %d"), i), int_span[i], i);
                TestEqual(*FString::Printf(TEXT("Float value at index %d"), i),
                          float_span[i],
                          static_cast<float>(i) * 10.0f);
            }
        });
    });
}

BEGIN_DEFINE_SPEC(FLockFreeMPSCQueueSoAInt32VectorSpec,
                  "Sandbox.LockFreeMPSCQueueSoA.Int32Vector",
                  EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
TArray<SoATestBatch<int32, FVector>> test_cases;
END_DEFINE_SPEC(FLockFreeMPSCQueueSoAInt32VectorSpec)

void FLockFreeMPSCQueueSoAInt32VectorSpec::Define() {
    test_cases = {
        {"Single batch with multiple values",
         {{{1, 2, 3}, {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}, {7.0, 8.0, 9.0}}}}},
        {"Multiple batches",
         {{{10, 20}, {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}}},
          {{30, 40}, {{0.0, 0.0, 1.0}, {1.0, 1.0, 0.0}}}}},
        {"Empty and non-empty batches",
         {{{100, 200}, {{1.0, 1.0, 1.0}, {2.0, 2.0, 2.0}}}, {{}, {}}, {{300}, {{3.0, 3.0, 3.0}}}}},
    };

    Describe("LockFreeMPSCQueueSoA<void, int32, FVector> - Basic operations", [this]() {
        for (auto const& test_case : test_cases) {
            It(*test_case.name,
               [this, batches = test_case.batches]() { run_soa_queue_test(this, batches); });
        }
    });

    Describe("swap_and_visit with FVector", [this]() {
        It("should invoke callable with correct spans", [this]() {
            ml::LockFreeMPSCQueueSoA<void, int32, FVector> queue{};
            (void)queue.init(3);

            (void)queue.enqueue(10, FVector{1.0, 0.0, 0.0});
            (void)queue.enqueue(20, FVector{0.0, 1.0, 0.0});

            int32 int_sum{0};
            FVector vec_sum{FVector::ZeroVector};
            queue.swap_and_visit([&](auto const& spans) {
                auto const& [int_span, vec_span]{spans};
                for (auto const value : int_span) {
                    int_sum += value;
                }
                for (auto const& vec : vec_span) {
                    vec_sum += vec;
                }
            });

            TestEqual(TEXT("Sum of ints"), int_sum, 30);
            TestEqual(TEXT("Sum of vectors"), vec_sum, FVector{1.0, 1.0, 0.0});
        });
    });

    Describe("Order preservation with FVector", [this]() {
        It("should maintain FIFO order", [this]() {
            ml::LockFreeMPSCQueueSoA<void, int32, FVector> queue{};
            (void)queue.init(5);

            TArray<int32> expected_ints{0, 1, 2, 3, 4};
            TArray<FVector> expected_vecs{{1.0, 0.0, 0.0},
                                          {0.0, 1.0, 0.0},
                                          {0.0, 0.0, 1.0},
                                          {1.0, 1.0, 0.0},
                                          {1.0, 1.0, 1.0}};

            for (int32 i{0}; i < expected_ints.Num(); ++i) {
                (void)queue.enqueue(expected_ints[i], expected_vecs[i]);
            }

            auto const [int_span, vec_span]{queue.swap_and_consume()};
            for (int32 i{0}; i < expected_ints.Num(); ++i) {
                TestEqual(
                    *FString::Printf(TEXT("Int at index %d"), i), int_span[i], expected_ints[i]);
                TestEqual(
                    *FString::Printf(TEXT("Vector at index %d"), i), vec_span[i], expected_vecs[i]);
            }
        });
    });
}

BEGIN_DEFINE_SPEC(FLockFreeMPSCQueueSoAThreeTypesSpec,
                  "Sandbox.LockFreeMPSCQueueSoA.ThreeTypes",
                  EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
TArray<SoATestBatch<int32, float, FVector>> test_cases;
END_DEFINE_SPEC(FLockFreeMPSCQueueSoAThreeTypesSpec)

void FLockFreeMPSCQueueSoAThreeTypesSpec::Define() {
    test_cases = {
        {"Single batch with three types",
         {{{100, 200}, {1.5f, 2.5f}, {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}}}}},
        {"Multiple batches with three types",
         {{{10, 20}, {1.0f, 2.0f}, {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}}},
          {{30, 40}, {3.0f, 4.0f}, {{0.0, 0.0, 1.0}, {1.0, 1.0, 0.0}}}}},
    };

    Describe("LockFreeMPSCQueueSoA<void, int32, float, FVector> - Basic operations", [this]() {
        for (auto const& test_case : test_cases) {
            It(*test_case.name,
               [this, batches = test_case.batches]() { run_soa_queue_test(this, batches); });
        }
    });

    Describe("Order preservation across all arrays", [this]() {
        It("should maintain FIFO order for all three types", [this]() {
            ml::LockFreeMPSCQueueSoA<void, int32, float, FVector> queue{};
            (void)queue.init(5);

            for (int32 i{0}; i < 5; ++i) {
                (void)queue.enqueue(
                    i, static_cast<float>(i) * 2.0f, FVector{static_cast<double>(i), 0.0, 0.0});
            }

            auto const [int_span, float_span, vec_span]{queue.swap_and_consume()};
            for (int32 i{0}; i < 5; ++i) {
                TestEqual(*FString::Printf(TEXT("Int at %d"), i), int_span[i], i);
                TestEqual(*FString::Printf(TEXT("Float at %d"), i),
                          float_span[i],
                          static_cast<float>(i) * 2.0f);
                TestEqual(*FString::Printf(TEXT("Vector at %d"), i),
                          vec_span[i],
                          FVector{static_cast<double>(i), 0.0, 0.0});
            }
        });
    });
}

// Custom view structs for testing alternative API
struct Int32FloatView {
    std::span<int32 const> int32_span;
    std::span<float const> float_span;
};

struct Int32VectorView {
    std::span<int32 const> int32_span;
    std::span<FVector const> vector_span;
};

BEGIN_DEFINE_SPEC(FLockFreeMPSCQueueSoACustomViewSpec,
                  "Sandbox.LockFreeMPSCQueueSoA.CustomView",
                  EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FLockFreeMPSCQueueSoACustomViewSpec)

void FLockFreeMPSCQueueSoACustomViewSpec::Define() {
    Describe("Custom view with Int32FloatView", [this]() {
        It("should enqueue and consume using custom view struct", [this]() {
            ml::LockFreeMPSCQueueSoA<Int32FloatView, int32, float> queue{};
            (void)queue.init(5);

            (void)queue.enqueue(10, 1.5f);
            (void)queue.enqueue(20, 2.5f);
            (void)queue.enqueue(30, 3.5f);

            auto const view{queue.swap_and_consume()};

            TEST_SPAN_EQUAL("Int32 span size", view.int32_span, 3);
            TEST_SPAN_EQUAL("Float span size", view.float_span, 3);

            TestEqual(TEXT("First int value"), view.int32_span[0], 10);
            TestEqual(TEXT("First float value"), view.float_span[0], 1.5f);
            TestEqual(TEXT("Second int value"), view.int32_span[1], 20);
            TestEqual(TEXT("Second float value"), view.float_span[1], 2.5f);
            TestEqual(TEXT("Third int value"), view.int32_span[2], 30);
            TestEqual(TEXT("Third float value"), view.float_span[2], 3.5f);
        });

        It("should maintain FIFO order with named view members", [this]() {
            ml::LockFreeMPSCQueueSoA<Int32FloatView, int32, float> queue{};
            (void)queue.init(10);

            for (int32 i{0}; i < 10; ++i) {
                (void)queue.enqueue(i * 100, static_cast<float>(i) + 0.5f);
            }

            auto const view{queue.swap_and_consume()};

            for (int32 i{0}; i < 10; ++i) {
                TestEqual(
                    *FString::Printf(TEXT("Int at index %d"), i), view.int32_span[i], i * 100);
                TestEqual(*FString::Printf(TEXT("Float at index %d"), i),
                          view.float_span[i],
                          static_cast<float>(i) + 0.5f);
            }
        });
    });

    Describe("swap_and_visit with custom view", [this]() {
        It("should invoke callable with custom view struct", [this]() {
            ml::LockFreeMPSCQueueSoA<Int32FloatView, int32, float> queue{};
            (void)queue.init(5);

            (void)queue.enqueue(100, 10.0f);
            (void)queue.enqueue(200, 20.0f);
            (void)queue.enqueue(300, 30.0f);

            int32 int_sum{0};
            float float_sum{0.0f};
            queue.swap_and_visit([&](Int32FloatView const& view) {
                for (auto const value : view.int32_span) {
                    int_sum += value;
                }
                for (auto const value : view.float_span) {
                    float_sum += value;
                }
            });

            TestEqual(TEXT("Sum of ints"), int_sum, 600);
            TestEqual(TEXT("Sum of floats"), float_sum, 60.0f);
        });

        It("should work with empty queue using custom view", [this]() {
            ml::LockFreeMPSCQueueSoA<Int32FloatView, int32, float> queue{};
            (void)queue.init(5);

            bool called{false};
            queue.swap_and_visit([&](Int32FloatView const& view) {
                called = true;
                TEST_SPAN_EQUAL("Empty int32 span", view.int32_span, 0);
                TEST_SPAN_EQUAL("Empty float span", view.float_span, 0);
            });

            TestTrue(TEXT("Callable was invoked"), called);
        });
    });

    Describe("Edge cases with custom view", [this]() {
        It("should return empty spans in custom view when queue is empty", [this]() {
            ml::LockFreeMPSCQueueSoA<Int32FloatView, int32, float> queue{};
            (void)queue.init(10);

            auto const view{queue.swap_and_consume()};
            TEST_SPAN_EQUAL("Empty int32 span size", view.int32_span, 0);
            TEST_SPAN_EQUAL("Empty float span size", view.float_span, 0);
        });

        It("should return empty view when called twice in a row", [this]() {
            ml::LockFreeMPSCQueueSoA<Int32FloatView, int32, float> queue{};
            (void)queue.init(10);

            (void)queue.enqueue(42, 3.14f);
            auto const view1{queue.swap_and_consume()};
            TEST_SPAN_EQUAL("First call int span size", view1.int32_span, 1);
            TEST_SPAN_EQUAL("First call float span size", view1.float_span, 1);

            auto const view2{queue.swap_and_consume()};
            TEST_SPAN_EQUAL("Second call int span size", view2.int32_span, 0);
            TEST_SPAN_EQUAL("Second call float span size", view2.float_span, 0);
        });

        It("should handle queue full behavior with custom view", [this]() {
            ml::LockFreeMPSCQueueSoA<Int32FloatView, int32, float> queue{};
            (void)queue.init(2);

            using enum ml::ELockFreeMPSCQueueEnqueueResult;

            TestEqual(TEXT("First enqueue"), queue.enqueue(1, 1.0f), Success);
            TestEqual(TEXT("Second enqueue"), queue.enqueue(2, 2.0f), Success);
            TestEqual(TEXT("Third enqueue (full)"), queue.enqueue(3, 3.0f), Full);

            auto const view{queue.swap_and_consume()};
            TEST_SPAN_EQUAL("View int span size", view.int32_span, 2);
            TEST_SPAN_EQUAL("View float span size", view.float_span, 2);

            TestEqual(TEXT("Enqueue after consume"), queue.enqueue(4, 4.0f), Success);
        });
    });

    Describe("Custom view with Int32VectorView", [this]() {
        It("should work with FVector using custom view", [this]() {
            ml::LockFreeMPSCQueueSoA<Int32VectorView, int32, FVector> queue{};
            (void)queue.init(3);

            (void)queue.enqueue(10, FVector{1.0, 0.0, 0.0});
            (void)queue.enqueue(20, FVector{0.0, 1.0, 0.0});
            (void)queue.enqueue(30, FVector{0.0, 0.0, 1.0});

            auto const view{queue.swap_and_consume()};

            TEST_SPAN_EQUAL("Int32 span size", view.int32_span, 3);
            TEST_SPAN_EQUAL("Vector span size", view.vector_span, 3);

            TestEqual(TEXT("First int"), view.int32_span[0], 10);
            TestEqual(TEXT("First vector"), view.vector_span[0], FVector{1.0, 0.0, 0.0});
            TestEqual(TEXT("Second int"), view.int32_span[1], 20);
            TestEqual(TEXT("Second vector"), view.vector_span[1], FVector{0.0, 1.0, 0.0});
            TestEqual(TEXT("Third int"), view.int32_span[2], 30);
            TestEqual(TEXT("Third vector"), view.vector_span[2], FVector{0.0, 0.0, 1.0});
        });

        It("should support swap_and_visit with Int32VectorView", [this]() {
            ml::LockFreeMPSCQueueSoA<Int32VectorView, int32, FVector> queue{};
            (void)queue.init(5);

            (void)queue.enqueue(5, FVector{1.0, 2.0, 3.0});
            (void)queue.enqueue(10, FVector{4.0, 5.0, 6.0});

            int32 int_sum{0};
            FVector vec_sum{FVector::ZeroVector};
            queue.swap_and_visit([&](Int32VectorView const& view) {
                for (auto const value : view.int32_span) {
                    int_sum += value;
                }
                for (auto const& vec : view.vector_span) {
                    vec_sum += vec;
                }
            });

            TestEqual(TEXT("Sum of ints"), int_sum, 15);
            TestEqual(TEXT("Sum of vectors"), vec_sum, FVector{5.0, 7.0, 9.0});
        });
    });
}
