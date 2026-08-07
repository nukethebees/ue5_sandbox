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
