#include <ioj/sim/telemetry/analysis.h>

#include <gtest/gtest.h>

#include <array>

namespace ioj::sim::telemetry {
namespace {
auto metric(Analysis const& analysis, Metric const id) -> MetricSeries const& {
    auto const& result{analysis.metrics.at(static_cast<std::size_t>(id))};
    EXPECT_EQ(result.metric, id);
    return result;
}
}

TEST(TelemetryAnalysis, EmptyAndSingleSampleRunsDoNotInventIntervals) {
    std::array<LevelTelemetryBattleSample, 1> samples{};
    for (std::size_t count{}; count <= samples.size(); ++count) {
        SCOPED_TRACE(count);
        auto const analysis{analyze({.battle_samples = std::span{samples}.first(count)})};
        ASSERT_EQ(analysis.metrics.size(), static_cast<std::size_t>(Metric::Count));
        for (auto const& series : analysis.metrics) {
            EXPECT_TRUE(series.values.empty());
            EXPECT_TRUE(series.simulated_elapsed_seconds.empty());
            EXPECT_FALSE(series.weighted_mean);
        }
        EXPECT_EQ(analysis.battle_shots.size(), count);
    }
}

TEST(TelemetryAnalysis, DerivesBattleForcesCombatAndProjectileRates) {
    std::array<LevelTelemetryBattleSample, 2> samples{};
    auto& begin{samples[0]};
    begin.alive[0][0] = 2;
    begin.combat.spawned[0][0] = 2;
    begin.active_lasers = 3;
    begin.lasers_fired = 10;
    auto& end{samples[1]};
    end = begin;
    end.completed_tick = 60;
    end.alive[0][0] = 3;
    end.combat.spawned[0][0] = 5;
    end.combat.shots[0][0] = 5;
    end.combat.hits[0][0] = 2;
    end.combat.damage_dealt[0][0] = 50;
    end.combat.destroyed[0][0] = 2;
    end.combat.kills[0][0] = 1;
    end.active_lasers = 4;
    end.lasers_fired = 15;

    auto const analysis{analyze({.battle_samples = samples})};
    auto const& active{metric(analysis, Metric::ActiveEntities)};
    EXPECT_EQ(active.simulated_elapsed_seconds, (std::vector<float>{1.f}));
    EXPECT_EQ(active.values, (std::vector<float>{3.f}));
    EXPECT_EQ(metric(analysis, Metric::SpawnRate).values, (std::vector<float>{3.f}));
    EXPECT_EQ(metric(analysis, Metric::DestructionRate).values, (std::vector<float>{2.f}));
    EXPECT_EQ(metric(analysis, Metric::KillRate).values, (std::vector<float>{1.f}));
    EXPECT_EQ(metric(analysis, Metric::LaserFireRate).values, (std::vector<float>{5.f}));
    EXPECT_EQ(metric(analysis, Metric::ActiveLasers).values, (std::vector<float>{4.f}));
    EXPECT_EQ(analysis.battle_shots, (std::vector<float>{0.f, 5.f}));
    EXPECT_EQ(analysis.battle_hits, (std::vector<float>{0.f, 2.f}));
    EXPECT_EQ(analysis.battle_damage_dealt, (std::vector<float>{0.f, 50.f}));
    EXPECT_EQ(analysis.battle_kills, (std::vector<float>{0.f, 1.f}));

    end.alive[0][0] = 2;
    auto const baseline{analyze({.battle_samples = samples})};
    auto const& baseline_active{metric(baseline, Metric::ActiveEntities)};
    EXPECT_EQ(baseline_active.simulated_elapsed_seconds, active.simulated_elapsed_seconds);
    EXPECT_EQ(baseline_active.values, (std::vector<float>{2.f}));
}

TEST(TelemetryAnalysis, SampledGaugesSurviveRegressingCounterIntervals) {
    std::array<LevelTelemetryBattleSample, 2> samples{};
    auto& begin{samples[0]};
    begin.completed_tick = 30;
    begin.alive[0][0] = 7;
    begin.combat.spawned[0][0] = 10;
    begin.combat.destroyed[0][0] = 10;
    begin.combat.kills[0][0] = 10;
    begin.lasers_fired = 10;
    auto& end{samples[1]};
    end = begin;
    end.completed_tick = 150;
    end.combat.spawned[0][0] = 5;
    end.combat.destroyed[0][0] = 5;
    end.combat.kills[0][0] = 5;
    end.lasers_fired = 5;

    auto const analysis{analyze({.battle_samples = samples})};
    EXPECT_EQ(metric(analysis, Metric::ActiveEntities).values, (std::vector<float>{7.f}));
    for (auto const id :
         {Metric::SpawnRate, Metric::DestructionRate, Metric::KillRate, Metric::LaserFireRate}) {
        EXPECT_TRUE(metric(analysis, id).values.empty());
        EXPECT_FALSE(metric(analysis, id).weighted_mean);
    }
}

TEST(TelemetryAnalysis, WeightsEntityCountsByIntervalDurationAcrossTeams) {
    std::array<LevelTelemetryBattleSample, 3> samples{};
    samples[1].completed_tick = 60;
    samples[2].completed_tick = 240;
    constexpr std::array types{Metric::PlayerShips,
                               Metric::Turrets,
                               Metric::CapitalShips,
                               Metric::CapitalShipFighters,
                               Metric::TubeSpinners};
    for (std::size_t type{}; type < types.size(); ++type) {
        samples[1].alive[0][type] = 1;
        samples[1].alive[1][type] = 2;
        samples[2].alive[0][type] = 5;
    }

    auto const analysis{analyze({.battle_samples = samples})};
    for (auto const id : types) {
        auto const& series{metric(analysis, id)};
        EXPECT_EQ(series.values, (std::vector<float>{3.f, 5.f}));
        ASSERT_TRUE(series.weighted_mean);
        EXPECT_DOUBLE_EQ(*series.weighted_mean, 4.5);
    }
    EXPECT_EQ(metric(analysis, Metric::ActiveEntities).weighted_mean, 22.5);
}

TEST(TelemetryAnalysis, IgnoresZeroAndRegressingDurationsWithoutLosingSamples) {
    std::array<LevelTelemetryBattleSample, 4> samples{};
    samples[0].completed_tick = 60;
    samples[1].completed_tick = 60;
    samples[2].completed_tick = 30;
    samples[3].completed_tick = 90;
    auto const analysis{analyze({.battle_samples = samples})};
    EXPECT_EQ(analysis.battle_simulated_seconds, (std::vector<float>{1.f, 1.f, .5f, 1.5f}));
    for (auto const& series : analysis.metrics) {
        EXPECT_EQ(series.simulated_elapsed_seconds, (std::vector<float>{1.5f}));
        EXPECT_EQ(series.values, (std::vector<float>{0.f}));
        EXPECT_EQ(series.weighted_mean, 0.);
    }
}
}
