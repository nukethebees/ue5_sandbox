#include <SandboxCore/container_concepts.h>
#include <SandboxCore/container_ops.h>
#include <SandboxCore/tick_countdown.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

#include <type_traits>


static_assert(std::is_same_v<FTickCountdown::size_type, int32>);
static_assert(std::is_same_v<FTickCountdown::counter_type, int16>);
static_assert(ml::SupportsNum<FTickCountdown>);
static_assert(ml::SupportsReset<FTickCountdown>);
static_assert(ml::SupportsReserve<FTickCountdown>);
static_assert(ml::SupportsAddUninitialised<FTickCountdown>);
static_assert(ml::SupportsAddDefaulted<FTickCountdown>);
static_assert(ml::SupportsRemoveAtSwap<FTickCountdown>);
static_assert(ml::SupportsSetNum<FTickCountdown>);
static_assert(ml::SupportsCopyElement<FTickCountdown>);
static_assert(ml::SupportsGetView<FTickCountdown>);

TEST_CASE("SandboxCore.TickCountdown.DefaultConstruction") {
    FTickCountdown countdown;

    CHECK(countdown.tick_value == 0);
    CHECK(countdown.num() == 0);

    countdown.tick();
    countdown.consume();

    CHECK(countdown.num() == 0);
}

TEST_CASE("SandboxCore.TickCountdown.SizedConstruction") {
    FTickCountdown countdown{3, 4};

    REQUIRE(countdown.num() == 3);
    CHECK(countdown.tick_value == 4);
    CHECK(countdown.counters[0] == 4);
    CHECK(countdown.counters[1] == 4);
    CHECK(countdown.counters[2] == 4);
}

TEST_CASE("SandboxCore.TickCountdown.TickDecrementsEveryCounter") {
    FTickCountdown countdown{3, 2};
    countdown.counters = {2, 0, -2};

    countdown.tick();

    CHECK(countdown.counters[0] == 1);
    CHECK(countdown.counters[1] == -1);
    CHECK(countdown.counters[2] == -3);
}

TEST_CASE("SandboxCore.TickCountdown.IsReadyChecksCounterValue") {
    CHECK_FALSE(FTickCountdown::is_ready(1));
    CHECK(FTickCountdown::is_ready(0));
    CHECK(FTickCountdown::is_ready(-1));
}

TEST_CASE("SandboxCore.TickCountdown.ConsumeIndexResetsOnlyReadyCounter") {
    FTickCountdown countdown{3, 5};
    countdown.counters = {1, 0, -1};

    countdown.consume(0);
    countdown.consume(1);
    countdown.consume(2);

    CHECK(countdown.counters[0] == 1);
    CHECK(countdown.counters[1] == 5);
    CHECK(countdown.counters[2] == 5);
}

TEST_CASE("SandboxCore.TickCountdown.TryConsumeReportsWhetherCounterWasReady") {
    FTickCountdown countdown{3, 5};
    countdown.counters = {1, 0, -1};

    CHECK_FALSE(countdown.try_consume(0));
    CHECK(countdown.counters[0] == 1);

    CHECK(countdown.try_consume(1));
    CHECK(countdown.counters[1] == 5);

    CHECK(countdown.try_consume(2));
    CHECK(countdown.counters[2] == 5);
}

TEST_CASE("SandboxCore.TickCountdown.ConsumeResetsAllReadyCounters") {
    FTickCountdown countdown{4, 3};
    countdown.counters = {2, 0, -4, 1};

    countdown.consume();

    CHECK(countdown.counters[0] == 2);
    CHECK(countdown.counters[1] == 3);
    CHECK(countdown.counters[2] == 3);
    CHECK(countdown.counters[3] == 1);
}

TEST_CASE("SandboxCore.TickCountdown.NonPositiveTickValueRemainsReadyAfterConsume") {
    FTickCountdown zero_countdown{1, 0};
    FTickCountdown negative_countdown{1, -2};

    zero_countdown.consume(0);
    negative_countdown.consume();

    CHECK(FTickCountdown::is_ready(zero_countdown.counters[0]));
    CHECK(FTickCountdown::is_ready(negative_countdown.counters[0]));
    CHECK(zero_countdown.counters[0] == 0);
    CHECK(negative_countdown.counters[0] == -2);
}

TEST_CASE("SandboxCore.TickCountdown.SupportsContainerOperations") {
    FTickCountdown countdown{2, 6};

    CHECK(ml::num(countdown) == 2);

    ml::reserve(countdown, 16);
    CHECK(ml::num(countdown) == 2);

    ml::reset(countdown);
    CHECK(ml::num(countdown) == 0);
    CHECK(countdown.tick_value == 6);
}

TEST_CASE("SandboxCore.TickCountdown.AddZeroedPreservesExistingCounters") {
    FTickCountdown countdown{2, 6};
    countdown.counters = {4, 2};

    countdown.add_zeroed(2);

    REQUIRE(countdown.num() == 4);
    CHECK(countdown.counters[0] == 4);
    CHECK(countdown.counters[1] == 2);
    CHECK(countdown.counters[2] == 0);
    CHECK(countdown.counters[3] == 0);
}

TEST_CASE("SandboxCore.TickCountdown.AddDefaultedPreservesExistingCounters") {
    FTickCountdown countdown{2, 6};
    countdown.counters = {4, 2};

    countdown.add_defaulted(2);

    REQUIRE(countdown.num() == 4);
    CHECK(countdown.counters[0] == 4);
    CHECK(countdown.counters[1] == 2);
    CHECK(countdown.counters[2] == 0);
    CHECK(countdown.counters[3] == 0);
}

TEST_CASE("SandboxCore.TickCountdown.AddUninitialisedPreservesExistingCounters") {
    FTickCountdown countdown{2, 6};
    countdown.counters = {4, 2};

    countdown.add_uninitialised(2);

    REQUIRE(countdown.num() == 4);
    CHECK(countdown.counters[0] == 4);
    CHECK(countdown.counters[1] == 2);
}

TEST_CASE("SandboxCore.TickCountdown.SupportsSoAOperations") {
    FTickCountdown source{3, 6};
    source.counters = {1, 2, 3};

    FTickCountdown destination{3, 6};
    destination.copy_element(1, source, 2);
    CHECK(destination.counters[1] == 3);

    auto slice{source.get_view(1, 2)};
    REQUIRE(slice.Num() == 2);
    CHECK(slice[0] == 2);
    CHECK(slice[1] == 3);

    auto const& const_source{source};
    auto const_slice{const_source.get_const_view(0, 2)};
    REQUIRE(const_slice.Num() == 2);
    CHECK(const_slice[0] == 1);
    CHECK(const_slice[1] == 2);

    source.remove_at_swap(0, 1, EAllowShrinking::No);
    REQUIRE(source.num() == 2);
    CHECK(source.counters[0] == 3);
    CHECK(source.counters[1] == 2);

    source.set_num(1, EAllowShrinking::No);
    REQUIRE(source.num() == 1);
    CHECK(source.counters[0] == 3);
}

TEST_CASE("SandboxCore.TickCountdown.GetViewAliasesCounters") {
    FTickCountdown countdown{3, 4};

    auto view{countdown.get_view()};
    static_assert(std::is_same_v<decltype(view), FTickCountdown::View>);
    REQUIRE(view.Num() == 3);

    view[1] = 2;
    CHECK(countdown.counters[1] == 2);

    view[2] = 0;
    CHECK(view.try_consume(2));
    CHECK(countdown.counters[2] == countdown.tick_value);

    view.reset(1);
    CHECK(countdown.counters[1] == countdown.tick_value);

    auto const& const_countdown{countdown};
    auto const_view{const_countdown.get_view()};
    static_assert(std::is_same_v<decltype(const_view), FTickCountdown::ConstView>);
    CHECK(const_view[1] == countdown.tick_value);
}
