#include <ioj/memory/bump_resource.h>
#include <ioj/sim/entities/entity_ledger.h>
#include <ioj/sim/lasers/sim.h>
#include <ioj/sim/sim_clock.h>
#include <ioj/sim/telemetry/level_telemetry_manager.h>
#include <ioj/sim/telemetry/report_json.h>
#include <ioj/sim/telemetry/run_report.h>

#include <gtest/gtest.h>

namespace ioj::sim::tests {
struct TelemetryFixture {
    explicit TelemetryFixture(telemetry::RecordingConfig config = {}, double rate = 60.0)
        : memory{config.enabled ? std::size_t{1024 * 1024} : 0}
        , manager{clock, ledger, lasers, memory, config} {
        clock.initialise({});
        clock.reset();
        clock.tick_loop.tick_rate = Frequency::make(rate);
        manager.initialise();
    }
    void tick(std::uint64_t value) {
        clock.completed_ticks = SimTick{value};
        manager.tick();
    }
    void spawn() {
        EntityId id;
        ledger.record_spawns(EntityType::Fighter, Team::Red, Health{1}, {&id, 1});
    }
    SimClock clock;
    EntityLedger ledger;
    lasers::Sim lasers;
    memory::BumpResource memory;
    LevelTelemetryManager manager;
};

TEST(LevelTelemetryManager, QuantizesTimeIntervalsOnceAndKeepsActualTicks) {
    TelemetryFixture fixture{{.interval = Duration::make(0.15)}, 10.0};
    EXPECT_EQ(fixture.manager.sampling_interval_ticks(), 2);
    fixture.tick(1);
    EXPECT_EQ(fixture.manager.recorded_sample_count(), 1);
    fixture.tick(2);
    fixture.tick(3);
    fixture.tick(4);
    auto const samples{fixture.manager.materialize_history()};
    ASSERT_EQ(samples.size(), 3);
    EXPECT_EQ(samples[0].completed_tick, SimTick{0});
    EXPECT_EQ(samples[1].completed_tick, SimTick{2});
    EXPECT_EQ(samples[2].completed_tick, SimTick{4});
    fixture.tick(5);
    fixture.manager.finalize(telemetry::RunEndReason::WorldEnd);
    EXPECT_EQ(fixture.manager.materialize_history().back().completed_tick, SimTick{5});
}

TEST(LevelTelemetryManager, DefaultsToOneHertzAndAllowsEveryTickSampling) {
    TelemetryFixture default_fixture;
    EXPECT_EQ(default_fixture.memory.capacity_bytes(), 1024 * 1024);
    EXPECT_EQ(default_fixture.manager.sampling_interval_ticks(), 60);
    default_fixture.tick(59);
    EXPECT_EQ(default_fixture.manager.recorded_sample_count(), 1);
    default_fixture.tick(60);
    EXPECT_EQ(default_fixture.manager.recorded_sample_count(), 2);

    TelemetryFixture every_tick{{.interval_ticks = 1}};
    for (std::uint64_t tick{1}; tick <= 4; ++tick) {
        every_tick.tick(tick);
    }
    EXPECT_EQ(every_tick.manager.recorded_sample_count(), 5);
}

TEST(LevelTelemetryManager, DurationIncludesItsEndpointAndDoesNotExtendHistory) {
    TelemetryFixture fixture{{.interval_ticks = 3, .duration = Duration::make(0.5)}, 10.0};
    for (std::uint64_t tick{1}; tick <= 10; ++tick) {
        fixture.tick(tick);
    }
    fixture.manager.finalize(telemetry::RunEndReason::WorldEnd);
    auto const samples{fixture.manager.materialize_history()};
    ASSERT_EQ(samples.size(), 3);
    EXPECT_EQ(samples[1].completed_tick, SimTick{3});
    EXPECT_EQ(samples[2].completed_tick, SimTick{5});
    auto const record{fixture.manager.take_finalized_run(fixture.clock.tick_rate())};
    ASSERT_TRUE(record);
    EXPECT_EQ(record->final_state.completed_tick, SimTick{10});
}

TEST(LevelTelemetryManager, DisabledRecordingRetainsLiveAndFinalResults) {
    TelemetryFixture fixture{{.enabled = false}};
    fixture.spawn();
    fixture.tick(1);
    EXPECT_EQ(fixture.memory.used_bytes(), 0);
    EXPECT_TRUE(fixture.manager.materialize_history().empty());
    auto const snapshot{fixture.manager.make_snapshot()};
    EXPECT_EQ(snapshot.active_entities, 1);
    EXPECT_EQ(snapshot.spawned_entities, 1);
    EXPECT_TRUE(snapshot.active_entity_count_data.is_empty());
    fixture.manager.finalize(telemetry::RunEndReason::WorldEnd);
    auto record{fixture.manager.take_finalized_run(fixture.clock.tick_rate())};
    ASSERT_TRUE(record);
    EXPECT_TRUE(record->battle_samples.empty());
    auto report{telemetry::make_run_report(std::move(*record), {}, false)};
    EXPECT_EQ(telemetry::summarize_run(report).spawned_entities, 1);
    auto serialized{telemetry::serialize_report(report)};
    ASSERT_TRUE(serialized) << serialized.error();
    auto decoded{telemetry::deserialize_report(*serialized)};
    ASSERT_TRUE(decoded) << decoded.error();
    EXPECT_EQ(telemetry::summarize_run(*decoded).active_entities, 1);
}

TEST(LevelTelemetryManager, DurationLimitPreservesFinalResultsAndNeverExtendsGraphs) {
    TelemetryFixture fixture{{.interval_ticks = 1, .duration = Duration::make(0.0)}};
    fixture.spawn();
    fixture.tick(1);
    fixture.spawn();
    fixture.tick(2);
    auto const snapshot{fixture.manager.make_snapshot()};
    EXPECT_EQ(snapshot.active_entities, 2);
    EXPECT_EQ(snapshot.active_entity_count_data.last_time(), 0);
    EXPECT_EQ(snapshot.active_entity_count_data.last_value(), 0);
    fixture.manager.finalize(telemetry::RunEndReason::WorldEnd);
    auto record{fixture.manager.take_finalized_run(fixture.clock.tick_rate())};
    ASSERT_TRUE(record);
    EXPECT_EQ(record->battle_samples.size(), 1);
    auto report{telemetry::make_run_report(std::move(*record), {}, true)};
    EXPECT_EQ(telemetry::summarize_run(report).active_entities, 2);
    auto serialized{telemetry::serialize_report(report)};
    ASSERT_TRUE(serialized);
    auto decoded{telemetry::deserialize_report(*serialized)};
    ASSERT_TRUE(decoded) << decoded.error();
    EXPECT_EQ(telemetry::summarize_run(*decoded).active_entities, 2);
}

TEST(LevelTelemetryManager, ResetReusesStorageAndRestartsSchedule) {
    TelemetryFixture fixture{{.interval_ticks = 1}};
    fixture.spawn();
    fixture.tick(1);
    fixture.manager.finalize(telemetry::RunEndReason::WorldEnd);
    EXPECT_EQ(fixture.manager.recorded_sample_count(), 2);
    auto const used_bytes{fixture.memory.used_bytes()};
    fixture.manager.reset();
    fixture.ledger.reset();
    fixture.clock.reset();
    EXPECT_EQ(fixture.memory.used_bytes(), used_bytes);
    fixture.manager.initialise();
    EXPECT_EQ(fixture.memory.used_bytes(), used_bytes);
    EXPECT_EQ(fixture.manager.recorded_sample_count(), 1);
    fixture.tick(1);
    EXPECT_EQ(fixture.manager.recorded_sample_count(), 2);
}
}
