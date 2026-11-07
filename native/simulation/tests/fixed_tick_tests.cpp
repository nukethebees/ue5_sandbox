#include "ioj/sim/fixed_tick.h"
#include "ioj/sim/fixed_tick_loop.h"
#include "ioj/sim/sim_time.h"
#include <ioj/sim/levels/simulation_settings.h>
#include <ioj/sim/sim_clock.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <limits>

namespace ioj::sim::tests {

TEST(FixedTick, InitialisesDerivedPeriodAndResetsAccumulator) {
    FixedTickLoop loop{.tick_rate = Frequency::make(10.0),
                       .time_scale = PositiveFinite<double>::make(2.0),
                       .accumulator = 1.0};

    loop.initialise();

    EXPECT_DOUBLE_EQ(loop.get_tick_period(), 0.1);
    EXPECT_DOUBLE_EQ(loop.accumulator, 0.0);
}

TEST(FixedTick, AccumulatesAndConsumesScaledTime) {
    FixedTickLoop loop{.tick_rate = Frequency::make(10.0),
                       .time_scale = PositiveFinite<double>::make(2.0)};
    loop.initialise();

    loop.add_time(0.03);
    EXPECT_FALSE(loop.try_tick());
    loop.add_time(0.03);
    EXPECT_TRUE(loop.try_tick());
    EXPECT_NEAR(loop.accumulator, 0.02, 1e-12);
}

TEST(FixedTick, MakesExactlyOneTickAvailable) {
    FixedTickLoop loop{.tick_rate = Frequency::make(10.0),
                       .time_scale = PositiveFinite<double>::make(2.0)};
    loop.initialise();

    loop.add_time(0.05);
    EXPECT_TRUE(loop.try_tick());
    EXPECT_FALSE(loop.try_tick());
    EXPECT_DOUBLE_EQ(loop.accumulator, 0.0);
}

TEST(FixedTick, ConsumesMultipleAccumulatedTicks) {
    FixedTickLoop loop{.tick_rate = Frequency::make(10.0)};
    loop.initialise();

    loop.add_time(0.35);
    EXPECT_TRUE(loop.try_tick());
    EXPECT_TRUE(loop.try_tick());
    EXPECT_TRUE(loop.try_tick());
    EXPECT_FALSE(loop.try_tick());
    EXPECT_NEAR(loop.accumulator, 0.05, 1e-12);
}

TEST(SimulationClock, ConvertsDurationsAndFrequencies) {
    EXPECT_EQ(frequency_to_tick_period(Frequency::make(60.0), Frequency::make(7.0)).get(), 9);
    EXPECT_EQ(duration_to_tick_period(Frequency::make(60.0), Duration::make(0.11)).get(), 7);
    EXPECT_DOUBLE_EQ(simulation_time(SimTick{12}, 0.25), 3.0);
}

TEST(SimulationClock, InitialisesRuntimeStateFromStaticSettings) {
    SimClock clock;
    clock.tick_loop.accumulator = 1.0;
    clock.completed_ticks = SimTick{17};
    clock.initialise(
        {.tick_rate = Frequency::make(10.0), .time_scale = PositiveFinite<double>::make(2.0)});

    EXPECT_DOUBLE_EQ(clock.tick_period(), 0.1);
    EXPECT_DOUBLE_EQ(clock.time_scale().get(), 2.0);
    EXPECT_DOUBLE_EQ(clock.tick_loop.accumulator, 0.0);
    EXPECT_EQ(clock.get_completed_ticks().value(), 0);

    clock.tick_loop.accumulator = 0.25;
    clock.completed_ticks = SimTick{3};
    clock.advance_to(SimulationPhase::Resolution);
    clock.reset();
    EXPECT_EQ(clock.phase(), SimulationPhase::BetweenTicks);
    EXPECT_EQ(clock.get_completed_ticks(), SimTick{});
    EXPECT_DOUBLE_EQ(clock.tick_loop.accumulator, 0.0);
    EXPECT_DOUBLE_EQ(clock.tick_period(), 0.1);
    EXPECT_DOUBLE_EQ(clock.time_scale().get(), 2.0);
}

TEST(SimulationSettings, AcceptsFrequenciesAboveTickRate) {
    SimulationSettings fast;
    for (auto const member : {&FighterSimConfig::avoidance_clear_update_frequency,
                              &FighterSimConfig::avoidance_update_frequency,
                              &FighterSimConfig::avoidance_active_update_frequency,
                              &FighterSimConfig::avoidance_immediate_update_frequency}) {
        fast.fighters.*member = PositiveFinite<float>{std::numeric_limits<float>::max()};
    }
    fast.turrets.target_refresh_frequency =
        ::ioj::PositiveFinite<float>::make(std::numeric_limits<float>::max());
    EXPECT_TRUE(validate_settings(fast));
}

TEST(SimulationSettings, ChecksCountdownFrequencyBoundsBeforeNarrowing) {
    auto check_boundary = [](auto assign, double const maximum, std::string_view const path) {
        SimulationSettings settings;
        settings.clock.tick_rate = Frequency::make(maximum);
        assign(settings, 1.f);
        EXPECT_TRUE(validate_settings(settings));

        settings.clock.tick_rate = Frequency::make(settings.clock.tick_rate.hz() + 0.25);
        auto const result{validate_settings(settings)};
        ASSERT_FALSE(result);
        EXPECT_TRUE(std::ranges::any_of(
            result.error(), [&](auto const& error) { return error.node_path == path; }));
    };
    check_boundary(
        [](auto& s, float value) {
            s.fighters.avoidance_clear_update_frequency = ::ioj::PositiveFinite<float>::make(value);
        },
        std::numeric_limits<std::int16_t>::max(),
        "fighters.avoidance-clear-update-frequency");
    check_boundary(
        [](auto& s, float value) {
            s.turrets.target_refresh_frequency = ::ioj::PositiveFinite<float>::make(value);
        },
        std::numeric_limits<std::int16_t>::max(),
        "turrets.target-refresh-frequency");
}

TEST(SimulationSettings, ChecksCountdownDurationBoundsBeforeNarrowing) {
    auto check_boundary = [](auto assign) {
        SimulationSettings settings;
        settings.clock.tick_rate = Frequency::make(1.0);
        assign(settings, static_cast<float>(std::numeric_limits<std::int16_t>::max()));
        EXPECT_TRUE(validate_settings(settings));
        assign(settings, 32767.5f);
        EXPECT_FALSE(validate_settings(settings));
        assign(settings, 0.f);
        EXPECT_TRUE(validate_settings(settings));
    };
    check_boundary([](auto& s, float value) {
        s.fighters.laser.fire_cooldown = ::ioj::NonNegativeFinite<float>::make(value);
    });
    check_boundary([](auto& s, float value) {
        s.fighters.attack_retry_cooldown = ::ioj::NonNegativeFinite<float>::make(value);
    });
    check_boundary([](auto& s, float value) {
        s.turrets.laser.fire_cooldown = ::ioj::NonNegativeFinite<float>::make(value);
    });
    check_boundary([](auto& s, float value) {
        s.spinners.laser.fire_cooldown = ::ioj::NonNegativeFinite<float>::make(value);
    });
}

TEST(SimulationSettings, ValidatesAuthoredClockAndOverride) {
    settings::Settings source;
    source.tick.tick_rate = Frequency{1.e9};
    EXPECT_FALSE(resolve_simulation_settings(source));
    EXPECT_FALSE(resolve_simulation_settings(source, settings::TickSettings{}));

    source.tick = {};
    EXPECT_FALSE(
        resolve_simulation_settings(source, settings::TickSettings{.tick_rate = Frequency{1.e9}}));
    auto const resolved{
        resolve_simulation_settings(source, settings::TickSettings{.tick_rate = Frequency{30.0}})};
    ASSERT_TRUE(resolved);
    EXPECT_EQ(resolved->clock.tick_rate, Frequency{30.0});
}

} // namespace tests
