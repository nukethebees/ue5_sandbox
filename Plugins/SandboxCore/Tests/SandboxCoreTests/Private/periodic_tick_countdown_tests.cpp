#include <SandboxCore/periodic_tick_countdown.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.PeriodicTickCountdown.UsesIndividualPeriods") {
    FPeriodicTickCountdown8 countdown;
    countdown.add_started(2);
    countdown.add_started(4);
    CHECK((countdown.periods == TArray<int8>{2, 4}));

    countdown.tick();
    CHECK_FALSE(countdown.is_ready(0));
    CHECK_FALSE(countdown.is_ready(1));
    countdown.tick();
    CHECK(countdown.try_consume(0));
    CHECK_FALSE(countdown.try_consume(1));
    CHECK((countdown.remaining_ticks == TArray<int8>{2, 2}));
}

TEST_CASE("SandboxCore.PeriodicTickCountdown.StartsReadyAndResets") {
    FPeriodicTickCountdown8 countdown;
    countdown.add_zeroed(1);

    CHECK(countdown.is_ready(0));
    CHECK(countdown.try_consume(0));
    CHECK(countdown.remaining_ticks[0] == 1);
    countdown.tick();
    CHECK(countdown.try_consume(0));
}

TEST_CASE("SandboxCore.PeriodicTickCountdown.StartedPeriodIsNotReady") {
    FPeriodicTickCountdown8 countdown;
    countdown.add_started(3);

    CHECK_FALSE(countdown.try_consume(0));
    countdown.tick();
    countdown.tick();
    CHECK_FALSE(countdown.try_consume(0));
    countdown.tick();
    CHECK(countdown.try_consume(0));
}

TEST_CASE("SandboxCore.PeriodicTickCountdown.AdvancesMultipleTicks") {
    FPeriodicTickCountdown8 countdown;
    countdown.add_started(3);
    countdown.add_started(7);

    countdown.tick(2);
    CHECK((countdown.remaining_ticks == TArray<int8>{1, 5}));

    countdown.tick(3);
    CHECK((countdown.remaining_ticks == TArray<int8>{0, 2}));
}

TEST_CASE("SandboxCore.PeriodicTickCountdown.ClampsLargeAdvancesToZero") {
    FPeriodicTickCountdown8 countdown;
    countdown.add_started(1);
    countdown.add_started(TNumericLimits<int8>::Max());

    countdown.tick(TNumericLimits<int8>::Max());

    CHECK((countdown.remaining_ticks == TArray<int8>{0, 0}));
}

TEST_CASE("SandboxCore.PeriodicTickCountdown.PreservesCounterPeriodPairsAcrossSoAOperations") {
    FPeriodicTickCountdown16 source;
    source.add_started(10);
    source.add_started(20);
    source.add_started(30);
    source.tick(3);

    FPeriodicTickCountdown16 destination;
    destination.add_zeroed(1, 3);
    destination.copy_element(0, source, 2);
    destination.copy_elements(1, source, 0, 2);

    CHECK((destination.remaining_ticks == TArray<int16>{27, 7, 17}));
    CHECK((destination.periods == TArray<int16>{30, 10, 20}));

    destination.remove_at_swap(1, 1, EAllowShrinking::No);
    CHECK((destination.remaining_ticks == TArray<int16>{27, 17}));
    CHECK((destination.periods == TArray<int16>{30, 20}));

    TArray<int32> permutation{1, 0};
    destination.apply_permutation(permutation);
    CHECK((destination.remaining_ticks == TArray<int16>{17, 27}));
    CHECK((destination.periods == TArray<int16>{20, 30}));
    CHECK((permutation == TArray<int32>{1, 0}));
}

TEST_CASE("SandboxCore.PeriodicTickCountdown.InitialiseLastPreservesEarlierRows") {
    FPeriodicTickCountdown16 countdown;
    countdown.add_started(10);
    countdown.add_started(20);
    countdown.add_uninitialised(2);

    countdown.initialise_last(30, 2);

    CHECK((countdown.remaining_ticks == TArray<int16>{10, 20, 0, 0}));
    CHECK((countdown.periods == TArray<int16>{10, 20, 30, 30}));
}

TEST_CASE("SandboxCore.PeriodicTickCountdown.OffsetViewsMutateOnlyTheirSourceRows") {
    FPeriodicTickCountdown16 countdown;
    countdown.add_started(10);
    countdown.add_zeroed(20);
    countdown.add_zeroed(30);
    countdown.add_started(40);

    auto view{countdown.get_view(1, 2)};
    auto nested_view{view.get_view(1, 1)};
    REQUIRE(view.num() == 2);
    REQUIRE(nested_view.num() == 1);
    CHECK(view.is_ready(0));
    CHECK(nested_view.try_consume(0));

    CHECK((countdown.remaining_ticks == TArray<int16>{10, 0, 30, 40}));
    CHECK((countdown.periods == TArray<int16>{10, 20, 30, 40}));

    auto const& const_countdown{countdown};
    auto const const_view{const_countdown.get_const_view(1, 2)};
    CHECK(const_view.is_ready(0));
    CHECK_FALSE(const_view.is_ready(1));
}

TEST_CASE("SandboxCore.PeriodicTickCountdown.ValidPeriodChecksIntegralBoundaries") {
    CHECK_FALSE(FPeriodicTickCountdown8::valid_period(int64{-1}));
    CHECK_FALSE(FPeriodicTickCountdown8::valid_period(int64{0}));
    CHECK(FPeriodicTickCountdown8::valid_period(int64{1}));
    CHECK(FPeriodicTickCountdown8::valid_period(int64{127}));
    CHECK_FALSE(FPeriodicTickCountdown8::valid_period(int64{128}));
    CHECK_FALSE(FPeriodicTickCountdown8::valid_period(uint64{128}));
    CHECK(FPeriodicTickCountdown16::valid_period(uint64{32767}));
    CHECK_FALSE(FPeriodicTickCountdown16::valid_period(uint64{32768}));
}
