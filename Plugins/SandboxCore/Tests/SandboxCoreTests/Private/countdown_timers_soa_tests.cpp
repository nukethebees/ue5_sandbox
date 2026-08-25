#include <SandboxCore/countdown_timers.h>

#include "TestHarness.h"

TEST_CASE("SandboxCore.CountdownTimers.TickAndMutableViews") {
    FCountdownTimers timers;
    timers.remaining_times = {1.f, 2.f, 3.f, 4.f};

    timers.tick(0.5f);
    auto middle{timers.slice(1, 2)};
    middle.remaining_times[0] -= 1.f;
    middle.remaining_times[1] = 10.f;

    CHECK((timers.remaining_times == TArray<float>{0.5f, 0.5f, 10.f, 3.5f}));

    auto const& const_timers{timers};
    auto const right{const_timers.right(2)};
    REQUIRE(right.num() == 2);
    CHECK(right.remaining_times[0] == 10.f);
    CHECK(right.remaining_times[1] == 3.5f);
}

TEST_CASE("SandboxCore.CountdownTimers.CopyAndAppendOperations") {
    FCountdownTimers source;
    source.remaining_times = {1.f, 2.f, 3.f};

    FCountdownTimers destination;
    destination.remaining_times = {10.f, 20.f, 30.f, 40.f};

    destination.copy_element(1, source, 2);
    CHECK((destination.remaining_times == TArray<float>{10.f, 3.f, 30.f, 40.f}));

    destination.copy_elements(2, source, 0, 2);
    CHECK((destination.remaining_times == TArray<float>{10.f, 3.f, 1.f, 2.f}));

    destination.copy_to_tail(source);
    CHECK((destination.remaining_times == TArray<float>{10.f, 1.f, 2.f, 3.f}));

    destination.append_from(source.get_const_view());
    CHECK((destination.remaining_times == TArray<float>{10.f, 1.f, 2.f, 3.f, 1.f, 2.f, 3.f}));
}

TEST_CASE("SandboxCore.CountdownTimers.SortAndPermutationPreserveScratchIndices") {
    FCountdownTimers timers;
    timers.remaining_times = {3.f, 1.f, 4.f, 2.f};
    TArray<int32> scratch_indices;
    scratch_indices.SetNumUninitialized(timers.num());

    timers.sort([](FCountdownTimers const& values,
                   int32 const lhs,
                   int32 const rhs) { return values.remaining_times[lhs] < values.remaining_times[rhs]; },
                scratch_indices);

    CHECK((timers.remaining_times == TArray<float>{1.f, 2.f, 3.f, 4.f}));
    CHECK((scratch_indices == TArray<int32>{1, 3, 0, 2}));
}

TEST_CASE("SandboxCore.CountdownTimers.ResizeRemoveAndReset") {
    FCountdownTimers timers;
    timers.reserve(6);
    timers.add_defaulted(3);
    REQUIRE(timers.num() == 3);
    CHECK((timers.remaining_times == TArray<float>{0.f, 0.f, 0.f}));

    timers.remaining_times = {1.f, 2.f, 3.f};
    timers.remove_at_swap(0, 1, EAllowShrinking::No);
    CHECK((timers.remaining_times == TArray<float>{3.f, 2.f}));

    timers.set_num(4, EAllowShrinking::No);
    CHECK(timers.num() == 4);
    timers.validate_array_sizes();

    timers.reset();
    CHECK(timers.is_empty());
}
