#include <SandboxCore/container_concepts.h>
#include <SandboxCore/container_ops.h>
#include <SandboxCore/tick_countdown.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

#include <initializer_list>
#include <type_traits>
#include <utility>

namespace {
template <typename T>
void set_counters(T& countdown, std::initializer_list<typename std::remove_cvref_t<T>::counter_type> const values) {
    check(countdown.num() == static_cast<int32>(values.size()));

    int32 index{0};
    for (auto const value : values) {
        countdown.set_counter(index++, value);
    }
}

template <typename T>
concept SupportsTryConsume = requires(T& value) { value.try_consume(0); };

template <typename T>
concept SupportsSetCounter = requires(T& value) { value.set_counter(0, typename T::counter_type{0}); };

template <typename T>
concept SupportsZeroCounter = requires(T& value) { value.zero_counter(0); };
}

static_assert(std::is_same_v<FTickCountdown::size_type, int32>);
static_assert(std::is_same_v<FTickCountdown::counter_type, int16>);
static_assert(std::is_same_v<TTickCountdown<int8>::counter_type, int8>);
static_assert(ml::SupportsNum<FTickCountdown>);
static_assert(ml::SupportsReset<FTickCountdown>);
static_assert(ml::SupportsReserve<FTickCountdown>);
static_assert(ml::SupportsAddUninitialised<FTickCountdown>);
static_assert(ml::SupportsAddDefaulted<FTickCountdown>);
static_assert(ml::SupportsRemoveAtSwap<FTickCountdown>);
static_assert(ml::SupportsSetNum<FTickCountdown>);
static_assert(ml::SupportsCopyElement<FTickCountdown>);
static_assert(ml::SupportsGetView<FTickCountdown>);
static_assert(SupportsTryConsume<FTickCountdown::View>);
static_assert(SupportsSetCounter<FTickCountdown::View>);
static_assert(SupportsZeroCounter<FTickCountdown::View>);
static_assert(!SupportsTryConsume<FTickCountdown::ConstView>);
static_assert(!SupportsSetCounter<FTickCountdown::ConstView>);
static_assert(!SupportsZeroCounter<FTickCountdown::ConstView>);
static_assert(std::is_same_v<decltype(std::declval<FTickCountdown&>().counters()), TConstArrayView<FTickCountdown::counter_type>>);

TEST_CASE("SandboxCore.TickCountdown.DefaultConstruction") {
    FTickCountdown countdown;

    CHECK(countdown.tick_value() == 0);
    CHECK(countdown.num() == 0);

    countdown.tick();
    countdown.consume();

    CHECK(countdown.num() == 0);
}

TEST_CASE("SandboxCore.TickCountdown.SizedConstruction") {
    FTickCountdown countdown{3, 4};

    REQUIRE(countdown.num() == 3);
    CHECK(countdown.tick_value() == 4);
    CHECK(countdown.counters()[0] == 4);
    CHECK(countdown.counters()[1] == 4);
    CHECK(countdown.counters()[2] == 4);
}

TEST_CASE("SandboxCore.TickCountdown.TickDecrementsEveryCounter") {
    FTickCountdown countdown{3, 2};
    set_counters(countdown, {2, 0, 0});

    countdown.tick();

    CHECK(countdown.counters()[0] == 1);
    CHECK(countdown.counters()[1] == -1);
    CHECK(countdown.counters()[2] == -1);
}

TEST_CASE("SandboxCore.TickCountdown.IsReadyChecksCounterValue") {
    CHECK_FALSE(FTickCountdown::is_ready(1));
    CHECK(FTickCountdown::is_ready(0));
    CHECK(FTickCountdown::is_ready(-1));
}

TEST_CASE("SandboxCore.TickCountdown.ConsumeIndexResetsOnlyReadyCounter") {
    FTickCountdown countdown{3, 5};
    set_counters(countdown, {2, 1, 0});
    countdown.tick();

    countdown.consume(0);
    countdown.consume(1);
    countdown.consume(2);

    CHECK(countdown.counters()[0] == 1);
    CHECK(countdown.counters()[1] == 5);
    CHECK(countdown.counters()[2] == 5);
}

TEST_CASE("SandboxCore.TickCountdown.TryConsumeReportsWhetherCounterWasReady") {
    FTickCountdown countdown{3, 5};
    set_counters(countdown, {2, 1, 0});
    countdown.tick();

    CHECK_FALSE(countdown.try_consume(0));
    CHECK(countdown.counters()[0] == 1);

    CHECK(countdown.try_consume(1));
    CHECK(countdown.counters()[1] == 5);

    CHECK(countdown.try_consume(2));
    CHECK(countdown.counters()[2] == 5);
}

TEST_CASE("SandboxCore.TickCountdown.ConsumeResetsAllReadyCounters") {
    FTickCountdown countdown{4, 3};
    set_counters(countdown, {3, 1, 0, 2});
    countdown.tick();

    countdown.consume();

    CHECK(countdown.counters()[0] == 2);
    CHECK(countdown.counters()[1] == 3);
    CHECK(countdown.counters()[2] == 3);
    CHECK(countdown.counters()[3] == 1);
}

TEST_CASE("SandboxCore.TickCountdown.ZeroTickValueRemainsReadyAfterConsume") {
    FTickCountdown zero_countdown{1, 0};

    zero_countdown.consume(0);

    CHECK(FTickCountdown::is_ready(zero_countdown.counters()[0]));
    CHECK(zero_countdown.counters()[0] == 0);
}

TEST_CASE("SandboxCore.TickCountdown.SetAndZeroCounter") {
    FTickCountdown countdown{2, 5};

    countdown.set_counter(0, 3);
    countdown.zero_counter(1);

    CHECK(countdown.counters()[0] == 3);
    CHECK(countdown.counters()[1] == 0);
}

TEST_CASE("SandboxCore.TickCountdown.Int8ClampsNegativeCountersEvery64Ticks") {
    TTickCountdown<int8> countdown{2, 100};
    countdown.zero_counter(0);

    for (int32 i{0}; i < 63; ++i) {
        countdown.tick();
    }

    CHECK(countdown.counters()[0] == -63);
    CHECK(countdown.counters()[1] == 37);

    countdown.tick();

    CHECK(countdown.counters()[0] == 0);
    CHECK(countdown.counters()[1] == 36);

    for (int32 i{0}; i < 64; ++i) {
        countdown.tick();
    }

    CHECK(countdown.counters()[0] == 0);
}

TEST_CASE("SandboxCore.TickCountdown.ResetRestartsCleanerCounter") {
    TTickCountdown<int8> countdown{1, 0};

    for (int32 i{0}; i < 32; ++i) {
        countdown.tick();
    }

    countdown.reset();
    countdown.add_zeroed(1);

    for (int32 i{0}; i < 63; ++i) {
        countdown.tick();
    }

    CHECK(countdown.counters()[0] == -63);

    countdown.tick();
    CHECK(countdown.counters()[0] == 0);
}

TEST_CASE("SandboxCore.TickCountdown.SupportsContainerOperations") {
    FTickCountdown countdown{2, 6};

    CHECK(ml::num(countdown) == 2);

    ml::reserve(countdown, 16);
    CHECK(ml::num(countdown) == 2);

    ml::reset(countdown);
    CHECK(ml::num(countdown) == 0);
    CHECK(countdown.tick_value() == 6);
}

TEST_CASE("SandboxCore.TickCountdown.AddZeroedPreservesExistingCounters") {
    FTickCountdown countdown{2, 6};
    set_counters(countdown, {4, 2});

    countdown.add_zeroed(2);

    REQUIRE(countdown.num() == 4);
    CHECK(countdown.counters()[0] == 4);
    CHECK(countdown.counters()[1] == 2);
    CHECK(countdown.counters()[2] == 0);
    CHECK(countdown.counters()[3] == 0);
}

TEST_CASE("SandboxCore.TickCountdown.AddDefaultedPreservesExistingCounters") {
    FTickCountdown countdown{2, 6};
    set_counters(countdown, {4, 2});

    countdown.add_defaulted(2);

    REQUIRE(countdown.num() == 4);
    CHECK(countdown.counters()[0] == 4);
    CHECK(countdown.counters()[1] == 2);
    CHECK(countdown.counters()[2] == 0);
    CHECK(countdown.counters()[3] == 0);
}

TEST_CASE("SandboxCore.TickCountdown.AddUninitialisedPreservesExistingCounters") {
    FTickCountdown countdown{2, 6};
    set_counters(countdown, {4, 2});

    countdown.add_uninitialised(2);

    REQUIRE(countdown.num() == 4);
    CHECK(countdown.counters()[0] == 4);
    CHECK(countdown.counters()[1] == 2);
}

TEST_CASE("SandboxCore.TickCountdown.SupportsSoAOperations") {
    FTickCountdown source{3, 6};
    set_counters(source, {1, 2, 3});

    FTickCountdown destination{3, 6};
    destination.copy_element(1, source, 2);
    CHECK(destination.counters()[1] == 3);

    auto slice{source.get_view(1, 2)};
    REQUIRE(slice.num() == 2);
    CHECK(slice[0] == 2);
    CHECK(slice[1] == 3);

    auto const& const_source{source};
    auto const_slice{const_source.get_const_view(0, 2)};
    REQUIRE(const_slice.num() == 2);
    CHECK(const_slice[0] == 1);
    CHECK(const_slice[1] == 2);

    source.remove_at_swap(0, 1, EAllowShrinking::No);
    REQUIRE(source.num() == 2);
    CHECK(source.counters()[0] == 3);
    CHECK(source.counters()[1] == 2);

    source.set_num(1, EAllowShrinking::No);
    REQUIRE(source.num() == 1);
    CHECK(source.counters()[0] == 3);
}

TEST_CASE("SandboxCore.TickCountdown.GetViewAliasesCounters") {
    FTickCountdown countdown{3, 4};

    auto view{countdown.get_view()};
    static_assert(std::is_same_v<decltype(view), FTickCountdown::View>);
    REQUIRE(view.num() == 3);

    view.set_counter(1, 2);
    CHECK(countdown.counters()[1] == 2);
    CHECK(view[1] == 2);

    view.zero_counter(2);
    CHECK(view.try_consume(2));
    CHECK(countdown.counters()[2] == countdown.tick_value());

    auto const& const_countdown{countdown};
    auto const_view{const_countdown.get_view()};
    static_assert(std::is_same_v<decltype(const_view), FTickCountdown::ConstView>);
    CHECK(const_view[1] == 2);
}
