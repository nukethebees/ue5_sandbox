#include <SandboxCore/periodic_countdown_timers.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.PeriodicCountdownTimers.AddStarted") {
    FPeriodicCountdownTimers timers;
    timers.add_started(1.f);
    timers.add_started(2.f, 2);

    TArray<float> const additional_periods{3.f, 4.f};
    timers.add_started(additional_periods);

    REQUIRE(timers.Num() == 5);
    CHECK((timers.remaining_times == TArray<float>{1.f, 2.f, 2.f, 3.f, 4.f}));
    CHECK((timers.periods == TArray<float>{1.f, 2.f, 2.f, 3.f, 4.f}));
}

TEST_CASE("SandboxCore.PeriodicCountdownTimers.AddZeroed") {
    FPeriodicCountdownTimers timers;
    timers.add_zeroed(1.f);
    timers.add_zeroed(2.f, 2);

    TArray<float> const additional_periods{3.f, 4.f};
    timers.add_zeroed(additional_periods);

    REQUIRE(timers.Num() == 5);
    CHECK((timers.remaining_times == TArray<float>{0.f, 0.f, 0.f, 0.f, 0.f}));
    CHECK((timers.periods == TArray<float>{1.f, 2.f, 2.f, 3.f, 4.f}));
}

TEST_CASE("SandboxCore.PeriodicCountdownTimers.TickAndConsume") {
    FPeriodicCountdownTimers timers;
    timers.add_started(1.f);
    timers.add_started(0.5f);
    timers.add_zeroed(2.f);

    timers.tick(0.5f);

    CHECK_FALSE(timers.expired(0));
    CHECK(timers.expired(1));
    CHECK(timers.expired(2));
    CHECK_FALSE(timers.try_consume(0));
    CHECK(timers.try_consume(1));
    CHECK(timers.remaining_times[1] == 0.5f);
    CHECK(timers.try_consume(2));
    CHECK(timers.remaining_times[2] == 2.f);

    timers.reset(0);
    CHECK(timers.remaining_times[0] == 1.f);
}

TEST_CASE("SandboxCore.PeriodicCountdownTimers.ConsumeDiscardsOvershoot") {
    FPeriodicCountdownTimers timers;
    timers.add_started(1.f);

    timers.tick(2.5f);

    CHECK(timers.remaining_times[0] == -1.5f);
    CHECK(timers.try_consume(0));
    CHECK(timers.remaining_times[0] == 1.f);
    CHECK_FALSE(timers.try_consume(0));

    timers.tick(1.f);
    CHECK(timers.remaining_times[0] == 0.f);
    CHECK(timers.try_consume(0));
    CHECK(timers.remaining_times[0] == 1.f);
}

TEST_CASE("SandboxCore.PeriodicCountdownTimers.SupportsSoAOperations") {
    FPeriodicCountdownTimers source;
    source.add_started(1.f);
    source.add_zeroed(2.f);
    source.add_started(3.f);

    FPeriodicCountdownTimers destination;
    destination.add_zeroed(4.f, 3);
    destination.copy_element(1, source, 2);

    CHECK(destination.remaining_times[1] == 3.f);
    CHECK(destination.periods[1] == 3.f);

    source.RemoveAtSwap(0, 1, EAllowShrinking::No);
    REQUIRE(source.Num() == 2);
    CHECK((source.remaining_times == TArray<float>{3.f, 0.f}));
    CHECK((source.periods == TArray<float>{3.f, 2.f}));

    TArray<float> const remaining_times{0.25f, 0.5f};
    TArray<float> const periods{5.f, 6.f};
    source.Append(remaining_times, periods);

    CHECK((source.remaining_times == TArray<float>{3.f, 0.f, 0.25f, 0.5f}));
    CHECK((source.periods == TArray<float>{3.f, 2.f, 5.f, 6.f}));

    auto const offset{source.AddUninitialized(1)};
    source.remaining_times[offset] = 0.75f;
    source.periods[offset] = 7.f;
    CHECK(source.Num() == 5);
    CHECK(source.remaining_times[offset] == 0.75f);
    CHECK(source.periods[offset] == 7.f);

    source.Reset();
    CHECK(source.Num() == 0);
    CHECK(source.periods.Num() == 0);
}
