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
concept SupportsRestartCounter = requires(T& value) {
    value.restart_counter(0);
    value.restart_counter(0, typename T::counter_type{0});
};

template <typename T>
concept SupportsSetCounter = requires(T& value) { value.set_counter(0, typename T::counter_type{0}); };

template <typename T>
concept SupportsZeroCounter = requires(T& value) { value.zero_counter(0); };
}

static_assert(std::is_same_v<FTickCountdown16::size_type, int32>);
static_assert(std::is_same_v<FTickCountdown16::counter_type, int16>);
static_assert(std::is_same_v<TTickCountdown<int8>::counter_type, int8>);
static_assert(ml::SupportsNum<FTickCountdown16>);
static_assert(ml::SupportsReset<FTickCountdown16>);
static_assert(ml::SupportsReserve<FTickCountdown16>);
static_assert(ml::SupportsAddUninitialised<FTickCountdown16>);
static_assert(ml::SupportsAddDefaulted<FTickCountdown16>);
static_assert(ml::SupportsRemoveAtSwap<FTickCountdown16>);
static_assert(ml::SupportsSetNum<FTickCountdown16>);
static_assert(ml::SupportsCopyElement<FTickCountdown16>);
static_assert(ml::SupportsGetView<FTickCountdown16>);
static_assert(SupportsTryConsume<FTickCountdown16::View>);
static_assert(SupportsRestartCounter<FTickCountdown16::View>);
static_assert(SupportsSetCounter<FTickCountdown16::View>);
static_assert(SupportsZeroCounter<FTickCountdown16::View>);
static_assert(!SupportsTryConsume<FTickCountdown16::ConstView>);
static_assert(!SupportsRestartCounter<FTickCountdown16::ConstView>);
static_assert(!SupportsSetCounter<FTickCountdown16::ConstView>);
static_assert(!SupportsZeroCounter<FTickCountdown16::ConstView>);
static_assert(std::is_same_v<decltype(std::declval<FTickCountdown16&>().counters()), TConstArrayView<FTickCountdown16::counter_type>>);

TEST_CASE("SandboxCore.TickCountdown.DefaultConstruction") {
    FTickCountdown16 countdown;

    CHECK(countdown.tick_value() == 0);
    CHECK(countdown.num() == 0);

    countdown.tick();
    countdown.consume();

    CHECK(countdown.num() == 0);
}

TEST_CASE("SandboxCore.TickCountdown.SizedConstruction") {
    FTickCountdown16 countdown{3, 4};

    REQUIRE(countdown.num() == 3);
    CHECK(countdown.tick_value() == 4);
    CHECK(countdown.counters()[0] == 4);
    CHECK(countdown.counters()[1] == 4);
    CHECK(countdown.counters()[2] == 4);
}

TEST_CASE("SandboxCore.TickCountdown.TickDecrementsEveryCounter") {
    FTickCountdown16 countdown{3, 2};
    set_counters(countdown, {2, 0, 0});

    countdown.tick();

    CHECK(countdown.counters()[0] == 1);
    CHECK(countdown.counters()[1] == -1);
    CHECK(countdown.counters()[2] == -1);
}

TEST_CASE("SandboxCore.TickCountdown.IsReadyChecksCounterValue") {
    CHECK_FALSE(FTickCountdown16::is_ready(1));
    CHECK(FTickCountdown16::is_ready(0));
    CHECK(FTickCountdown16::is_ready(-1));
}

TEST_CASE("SandboxCore.TickCountdown.TickCanFitHandlesDifferentIntegralTypes") {
    CHECK(FTickCountdown8::tick_can_fit(int64{0}));
    CHECK(FTickCountdown8::tick_can_fit(uint64{127}));
    CHECK_FALSE(FTickCountdown8::tick_can_fit(int64{-1}));
    CHECK_FALSE(FTickCountdown8::tick_can_fit(uint64{128}));
    CHECK(FTickCountdown16::tick_can_fit(uint64{32767}));
    CHECK_FALSE(FTickCountdown16::tick_can_fit(uint64{32768}));
}

TEST_CASE("SandboxCore.TickCountdown.SetTickValueAcceptsDifferentIntegralTypes") {
    FTickCountdown16 countdown;

    countdown.set_tick_value(uint64{32767});

    CHECK(countdown.tick_value() == 32767);
}

TEST_CASE("SandboxCore.TickCountdown.ConsumeIndexResetsOnlyReadyCounter") {
    FTickCountdown16 countdown{3, 5};
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
    FTickCountdown16 countdown{3, 5};
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
    FTickCountdown16 countdown{4, 3};
    set_counters(countdown, {3, 1, 0, 2});
    countdown.tick();

    countdown.consume();

    CHECK(countdown.counters()[0] == 2);
    CHECK(countdown.counters()[1] == 3);
    CHECK(countdown.counters()[2] == 3);
    CHECK(countdown.counters()[3] == 1);
}

TEST_CASE("SandboxCore.TickCountdown.ZeroTickValueRemainsReadyAfterConsume") {
    FTickCountdown16 zero_countdown{1, 0};

    zero_countdown.consume(0);

    CHECK(FTickCountdown16::is_ready(zero_countdown.counters()[0]));
    CHECK(zero_countdown.counters()[0] == 0);
}

TEST_CASE("SandboxCore.TickCountdown.SetAndZeroCounter") {
    FTickCountdown16 countdown{2, 5};

    countdown.set_counter(0, 3);
    countdown.zero_counter(1);

    CHECK(countdown.counters()[0] == 3);
    CHECK(countdown.counters()[1] == 0);
}

TEST_CASE("SandboxCore.TickCountdown.RestartCounterUsesDefaultOrExplicitTickValue") {
    FTickCountdown16 countdown{2, 5};
    countdown.zero_counter(0);
    countdown.zero_counter(1);

    countdown.restart_counter(0);
    countdown.restart_counter(1, 3);

    CHECK(countdown.counters()[0] == 5);
    CHECK(countdown.counters()[1] == 3);
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
    FTickCountdown16 countdown{2, 6};

    CHECK(ml::num(countdown) == 2);

    ml::reserve(countdown, 16);
    CHECK(ml::num(countdown) == 2);

    ml::reset(countdown);
    CHECK(ml::num(countdown) == 0);
    CHECK(countdown.tick_value() == 6);
}

TEST_CASE("SandboxCore.TickCountdown.AddZeroedPreservesExistingCounters") {
    FTickCountdown16 countdown{2, 6};
    set_counters(countdown, {4, 2});

    countdown.add_zeroed(2);

    REQUIRE(countdown.num() == 4);
    CHECK(countdown.counters()[0] == 4);
    CHECK(countdown.counters()[1] == 2);
    CHECK(countdown.counters()[2] == 0);
    CHECK(countdown.counters()[3] == 0);
}

TEST_CASE("SandboxCore.TickCountdown.AddDefaultedPreservesExistingCounters") {
    FTickCountdown16 countdown{2, 6};
    set_counters(countdown, {4, 2});

    countdown.add_defaulted(2);

    REQUIRE(countdown.num() == 4);
    CHECK(countdown.counters()[0] == 4);
    CHECK(countdown.counters()[1] == 2);
    CHECK(countdown.counters()[2] == 0);
    CHECK(countdown.counters()[3] == 0);
}

TEST_CASE("SandboxCore.TickCountdown.AddUninitialisedPreservesExistingCounters") {
    FTickCountdown16 countdown{2, 6};
    set_counters(countdown, {4, 2});

    countdown.add_uninitialised(2);

    REQUIRE(countdown.num() == 4);
    CHECK(countdown.counters()[0] == 4);
    CHECK(countdown.counters()[1] == 2);
}

TEST_CASE("SandboxCore.TickCountdown.SupportsSoAOperations") {
    FTickCountdown16 source{3, 6};
    set_counters(source, {1, 2, 3});

    FTickCountdown16 destination{3, 6};
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
    FTickCountdown16 countdown{3, 4};

    auto view{countdown.get_view()};
    static_assert(std::is_same_v<decltype(view), FTickCountdown16::View>);
    REQUIRE(view.num() == 3);

    view.set_counter(1, 2);
    CHECK(countdown.counters()[1] == 2);
    CHECK(view[1] == 2);

    view.restart_counter(1);
    CHECK(view[1] == countdown.tick_value());

    view.zero_counter(2);
    CHECK(view.try_consume(2));
    CHECK(countdown.counters()[2] == countdown.tick_value());

    auto const& const_countdown{countdown};
    auto const_view{const_countdown.get_view()};
    static_assert(std::is_same_v<decltype(const_view), FTickCountdown16::ConstView>);
    CHECK(const_view[1] == countdown.tick_value());
}
