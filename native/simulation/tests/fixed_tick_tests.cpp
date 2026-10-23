#include "ioj/sim/fixed_tick.h"
#include "ioj/sim/fixed_tick_loop.h"
#include "ioj/sim/sim_time.h"
#include <ioj/sim/levels/simulation_settings.h>

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

TEST(FixedTick, RejectsInvalidSettingsWithoutChangingAccumulator) {
    double accumulator{1.0};
    EXPECT_FALSE(Frequency::try_make(0.0));
    EXPECT_FALSE(PositiveFinite<double>::try_make(0.0));
    EXPECT_DOUBLE_EQ(accumulator, 1.0);
}

TEST(SimulationClock, ConvertsDurationsAndFrequencies) {
    EXPECT_EQ(frequency_to_tick_period(Frequency::make(60.0), Frequency::make(7.0)).get(), 9);
    EXPECT_EQ(duration_to_tick_period(Frequency::make(60.0), Duration::make(0.11)).get(), 7);
    EXPECT_DOUBLE_EQ(simulation_time(12, 0.25), 3.0);
}

TEST(SimulationSettings, RequiresFinitePositiveFrequencies) {
    auto check_frequency = [](auto assign) {
        for (auto const invalid : {0.f,
                                   -1.f,
                                   std::numeric_limits<float>::infinity(),
                                   std::numeric_limits<float>::quiet_NaN()}) {
            SimulationSettings settings;
            assign(settings, invalid);
            EXPECT_FALSE(validate_settings(settings));
        }
    };
    SimulationSettings fast;
    for (auto const member : {&FighterSimConfig::awareness_scan_frequency,
                              &FighterSimConfig::attack_reposition_frequency,
                              &FighterSimConfig::avoidance_clear_update_frequency,
                              &FighterSimConfig::avoidance_update_frequency,
                              &FighterSimConfig::avoidance_active_update_frequency,
                              &FighterSimConfig::avoidance_immediate_update_frequency}) {
        check_frequency([=](auto& settings, float value) { settings.fighters.*member = value; });
        fast.fighters.*member = std::numeric_limits<float>::max();
    }
    check_frequency(
        [](auto& settings, float value) { settings.turrets.target_refresh_frequency = value; });
    fast.turrets.target_refresh_frequency = std::numeric_limits<float>::max();
    EXPECT_TRUE(validate_settings(fast));
}

TEST(SimulationSettings, ChecksCountdownFrequencyBoundsBeforeNarrowing) {
    auto check_boundary = [](auto assign, double const maximum, std::string_view const path) {
        SimulationSettings settings;
        settings.clock.tick_rate = Frequency::make(maximum);
        settings.fighters.awareness_scan_frequency = static_cast<float>(maximum);
        assign(settings, 1.f);
        EXPECT_TRUE(validate_settings(settings));

        settings.clock.tick_rate = Frequency::make(settings.clock.tick_rate.get_hz() + 0.25);
        auto const result{validate_settings(settings)};
        ASSERT_FALSE(result);
        EXPECT_TRUE(std::ranges::any_of(
            result.error(), [&](auto const& error) { return error.node_path == path; }));
    };
    check_boundary([](auto& s, float value) { s.fighters.awareness_scan_frequency = value; },
                   std::numeric_limits<std::int8_t>::max(),
                   "fighters.awareness-scan-frequency");
    check_boundary([](auto& s, float value) { s.fighters.attack_reposition_frequency = value; },
                   std::numeric_limits<std::int16_t>::max(),
                   "fighters.attack-reposition-frequency");
    check_boundary(
        [](auto& s, float value) { s.fighters.avoidance_clear_update_frequency = value; },
        std::numeric_limits<std::int16_t>::max(),
        "fighters.avoidance-clear-update-frequency");
    check_boundary([](auto& s, float value) { s.turrets.target_refresh_frequency = value; },
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
        assign(settings, -1.f);
        EXPECT_FALSE(validate_settings(settings));
        assign(settings, 0.f);
        EXPECT_TRUE(validate_settings(settings));
    };
    check_boundary([](auto& s, float value) { s.fighters.laser.fire_cooldown = value; });
    check_boundary([](auto& s, float value) { s.fighters.attack_retry_cooldown = value; });
    check_boundary([](auto& s, float value) { s.turrets.laser.fire_cooldown = value; });
    check_boundary([](auto& s, float value) { s.spinners.laser.fire_cooldown = value; });
}

} // namespace tests
