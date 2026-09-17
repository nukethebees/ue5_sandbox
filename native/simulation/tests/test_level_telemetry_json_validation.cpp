#include <ioj/sim/telemetry/level_telemetry_json_validation.h>

#include <gtest/gtest.h>

#include <limits>

namespace ioj::sim::telemetry::tests {

TEST(LevelTelemetryJsonValidation, ParsesOnlyExactInRangeIntegers) {
    EXPECT_EQ(*parse_json_int32(42.0, "value"), 42);
    EXPECT_FALSE(parse_json_int32(1.5, "value").has_value());
    EXPECT_FALSE(parse_json_int32(
                     static_cast<double>(std::numeric_limits<std::int32_t>::max()) + 1.0, "value")
                     .has_value());
    EXPECT_EQ(*parse_json_uint64(9007199254740992.0, "value"), 9007199254740992ULL);
    EXPECT_FALSE(parse_json_uint64(9007199254740994.0, "value").has_value());
}

TEST(LevelTelemetryJsonValidation, RejectsInvalidRecordInvariants) {
    LevelTelemetryRunRecord record;
    record.metadata.tick_rate_hz = 60.0;
    record.metadata.tick_period_seconds = 1.0 / 60.0;
    record.metadata.initial_requested_time_scale = 1.0;

    EXPECT_TRUE(validate_level_telemetry_json_record(record).has_value());

    record.completion.mission_mode = MissionMode::None;
    EXPECT_FALSE(validate_level_telemetry_json_record(record).has_value());
    record.completion.mission_mode.reset();

    record.tick_series.active_entities.add(1, -1);
    EXPECT_FALSE(validate_level_telemetry_json_record(record).has_value());
    record.tick_series.active_entities = {};

    record.battle_samples.push_back({.completed_tick = 9007199254740994ULL});
    EXPECT_FALSE(validate_level_telemetry_json_record(record).has_value());
}

} // namespace ioj::sim::telemetry::tests
