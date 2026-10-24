#include <ioj/sim/telemetry/json_validation.h>
#include <ioj/sim/telemetry/report_json.h>

#include <gtest/gtest.h>

#include <limits>

namespace ioj::sim::telemetry::tests {

TEST(LevelTelemetryJsonValidation, ValidatesFiniteJsonNumbers) {
    EXPECT_TRUE(is_valid_json_number(0.0, false));
    EXPECT_TRUE(is_valid_json_number(0.0, true));
    EXPECT_FALSE(is_valid_json_number(-1.0, true));
    EXPECT_FALSE(is_valid_json_number(std::numeric_limits<double>::infinity(), false));
    EXPECT_FALSE(is_valid_json_number(std::numeric_limits<double>::quiet_NaN(), false));
}

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

    EXPECT_TRUE(validate_level_telemetry_json_record(record).has_value());

    record.tick_series.active_entities.add(1, -1);
    EXPECT_FALSE(validate_level_telemetry_json_record(record).has_value());
    record.tick_series.active_entities = {};

    record.battle_samples.push_back({.completed_tick = 9007199254740994ULL});
    EXPECT_FALSE(validate_level_telemetry_json_record(record).has_value());
}

TEST(LevelTelemetryJsonValidation, RoundTripsNativeReportAndMissionCompletion) {
    LevelTelemetryReport report;
    report.metadata.run_id = "run-1";
    report.metadata.level_display_name = "Convoy \"alpha\"\n\xc3\xa9";
    report.environment.project_name = "Space Game";
    report.environment.physical_core_count = 8;
    report.environment.total_physical_memory_bytes = 32ULL << 30;
    report.presentation_enabled = true;
    report.presentation_mode = "visual";
    report.tick_rate_hz = Frequency::make(120.0);
    report.completion.reason = LevelTelemetryRunEndReason::MissionSucceeded;
    report.completion.completed_ticks = 240;
    report.tick_series.active_entities.add(0, 2);
    report.tick_series.active_entities.add(240, 1);
    LevelTelemetryBattleSample sample;
    sample.completed_tick = 240;
    sample.alive[0][0] = 1;
    sample.combat.shots[0][0] = 3;
    sample.combat.damage_dealt[0][0] = 12.5;
    report.battle_samples.push_back(sample);

    auto const serialized{serialize_level_telemetry_report(report)};
    ASSERT_TRUE(serialized.has_value()) << serialized.error();
    auto const parsed{deserialize_level_telemetry_report(*serialized)};
    ASSERT_TRUE(parsed.has_value()) << parsed.error();
    EXPECT_EQ(parsed->metadata.level_display_name, report.metadata.level_display_name);
    EXPECT_EQ(parsed->environment.project_name, report.environment.project_name);
    EXPECT_EQ(parsed->environment.total_physical_memory_bytes, 32ULL << 30);
    EXPECT_TRUE(parsed->presentation_enabled);
    EXPECT_EQ(parsed->tick_rate_hz.get_hz(), 120.0);
    EXPECT_EQ(parsed->completion.reason, LevelTelemetryRunEndReason::MissionSucceeded);
    EXPECT_EQ(parsed->completion.completed_ticks, 240);
    EXPECT_EQ(serialized->find("winning_team"), std::string::npos);
    EXPECT_EQ(serialized->find("objective_id"), std::string::npos);
    EXPECT_EQ(parsed->tick_series.active_entities.num(), 2);
    EXPECT_EQ(parsed->tick_series.active_entities.last_value(), 1);
    ASSERT_EQ(parsed->battle_samples.size(), 1);
    EXPECT_EQ(parsed->battle_samples[0].combat.shots, sample.combat.shots);
    EXPECT_EQ(parsed->battle_samples[0].combat.damage_dealt, sample.combat.damage_dealt);
}

TEST(LevelTelemetryJsonValidation, NativeCodecRejectsInvalidInputAndReturnsErrors) {
    EXPECT_FALSE(deserialize_level_telemetry_report("{").has_value());
    EXPECT_FALSE(deserialize_level_telemetry_report("[]").has_value());
    EXPECT_FALSE(deserialize_level_telemetry_report(R"({"schema_version":8})").has_value());
    LevelTelemetryReport report;
    auto const empty{serialize_level_telemetry_report(report)};
    ASSERT_TRUE(empty.has_value());
    EXPECT_TRUE(deserialize_level_telemetry_report(*empty).has_value());

    report.environment.physical_core_count = -1;
    EXPECT_FALSE(serialize_level_telemetry_report(report).has_value());
    report.environment.physical_core_count = 0;
    report.completion.completed_ticks = 9007199254740993ULL;
    EXPECT_FALSE(serialize_level_telemetry_report(report).has_value());
    report.completion.completed_ticks = 0;
    report.completion.reason = static_cast<LevelTelemetryRunEndReason>(255);
    EXPECT_FALSE(serialize_level_telemetry_report(report).has_value());
}

} // namespace ioj::sim::telemetry::tests
