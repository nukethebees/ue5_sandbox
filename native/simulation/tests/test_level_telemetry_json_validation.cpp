#include <ioj/sim/telemetry/json_validation.h>
#include <ioj/sim/telemetry/report_json.h>
#include <ioj/sim/telemetry/run_report.h>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

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
    telemetry::RunRecord record;

    EXPECT_TRUE(validate_level_telemetry_json_record(record).has_value());

    record.tick_series.active_entities.add(1, -1);
    EXPECT_FALSE(validate_level_telemetry_json_record(record).has_value());
    record.tick_series.active_entities = {};

    record.battle_samples.push_back({.completed_tick = SimTick{9007199254740994ULL}});
    EXPECT_FALSE(validate_level_telemetry_json_record(record).has_value());
}

TEST(LevelTelemetryJsonValidation, RoundTripsNativeReportAndMissionCompletion) {
    auto report{make_run_report({},
                                {.launch_mode = levels::LaunchMode::Paused,
                                 .results_navigation = levels::ResultsNavigation::Telemetry},
                                true)};
    report.run.metadata.run_id = "run-1";
    report.run.metadata.level_display_name = "Convoy \"alpha\"\n\xc3\xa9";
    report.run.tick_rate_hz = Frequency::make(120.0);
    report.run.completion.reason = telemetry::RunEndReason::MissionSucceeded;
    report.run.completion.completed_ticks = SimTick{240};
    report.run.tick_series.active_entities.add(0, 2);
    report.run.tick_series.active_entities.add(240, 1);
    telemetry::BattleSample sample;
    sample.completed_tick = SimTick{240};
    sample.alive[0][0] = 1;
    sample.combat.shots[0][0] = 3;
    sample.combat.damage_dealt[0][0] = 12.5;
    report.run.battle_samples.push_back(sample);

    auto const serialized{telemetry::serialize_report(report)};
    ASSERT_TRUE(serialized.has_value()) << serialized.error();
    auto const parsed{telemetry::deserialize_report(*serialized)};
    ASSERT_TRUE(parsed.has_value()) << parsed.error();
    EXPECT_EQ(parsed->run.metadata.level_display_name, report.run.metadata.level_display_name);
    EXPECT_TRUE(parsed->presentation_enabled);
    EXPECT_TRUE(parsed->started_paused);
    EXPECT_TRUE(parsed->opens_telemetry);
    EXPECT_EQ(parsed->run.tick_rate_hz.hz(), 120.0);
    EXPECT_EQ(parsed->run.completion.reason, telemetry::RunEndReason::MissionSucceeded);
    EXPECT_EQ(parsed->run.completion.completed_ticks.value(), 240);
    EXPECT_EQ(serialized->find("environment"), std::string::npos);
    EXPECT_EQ(serialized->find("presentation_mode"), std::string::npos);
    EXPECT_EQ(serialized->find("winning_team"), std::string::npos);
    EXPECT_EQ(serialized->find("objective_id"), std::string::npos);
    EXPECT_EQ(parsed->run.tick_series.active_entities.num(), 2);
    EXPECT_EQ(parsed->run.tick_series.active_entities.last_value(), 1);
    ASSERT_EQ(parsed->run.battle_samples.size(), 1);
    EXPECT_EQ(parsed->run.battle_samples[0].combat.shots, sample.combat.shots);
    EXPECT_EQ(parsed->run.battle_samples[0].combat.damage_dealt, sample.combat.damage_dealt);
    auto const summary{summarize_run(*parsed)};
    EXPECT_EQ(summary.simulated_seconds, 2.0);
    EXPECT_EQ(summary.active_entities, 1);
    EXPECT_TRUE(summary.mission_complete);
    EXPECT_EQ(summary.shots, 3);
    EXPECT_EQ(summary.damage_dealt, 12.5);

    auto const reject_number{[&](std::string const& field, std::string const& replacement) {
        SCOPED_TRACE(field + " = " + replacement);
        auto invalid{*serialized};
        auto const offset{invalid.find(field)};
        ASSERT_NE(offset, std::string::npos);
        invalid.replace(offset, field.size(), replacement);
        EXPECT_FALSE(telemetry::deserialize_report(invalid));
    }};
    reject_number("\"active_lasers\": 0", "\"active_lasers\": -1");
    reject_number("\"completed_ticks\": 240", "\"completed_ticks\": 1.5");
    reject_number("\"completed_ticks\": 240", "\"completed_ticks\": 9007199254740993");
}

TEST(LevelTelemetryJsonValidation, NativeCodecRejectsInvalidInputAndReturnsErrors) {
    EXPECT_FALSE(telemetry::deserialize_report("{").has_value());
    EXPECT_FALSE(telemetry::deserialize_report("[]").has_value());
    EXPECT_FALSE(telemetry::deserialize_report(R"({"schema_version":10})").has_value());
    telemetry::Report report;
    auto const empty{telemetry::serialize_report(report)};
    ASSERT_TRUE(empty.has_value());
    EXPECT_TRUE(telemetry::deserialize_report(*empty).has_value());

    for (auto const field : {"launch_state", "results_navigation"}) {
        SCOPED_TRACE(field);
        auto invalid{*empty};
        auto const key{'"' + std::string{field} + "\": \""};
        auto const offset{invalid.find(key)};
        ASSERT_NE(offset, std::string::npos);
        invalid.insert(offset + key.size(), "unknown-");
        EXPECT_FALSE(telemetry::deserialize_report(invalid));
    }

    report.run.completion.completed_ticks = SimTick{9007199254740993ULL};
    EXPECT_FALSE(telemetry::serialize_report(report).has_value());
    report.run.completion.completed_ticks = SimTick{0};
    report.run.completion.reason = static_cast<telemetry::RunEndReason>(255);
    EXPECT_FALSE(telemetry::serialize_report(report).has_value());
}

TEST(LevelTelemetryJsonValidation, RejectsCorruptSparseSeriesAndBattleSamples) {
    telemetry::Report report;
    report.run.completion.completed_ticks = SimTick{1};
    report.run.tick_series.active_entities.add(0, 1);
    report.run.tick_series.active_entities.add(1, 0);
    report.run.battle_samples.push_back({.completed_tick = SimTick{1}, .active_lasers = 3});
    auto const serialized{telemetry::serialize_report(report)};
    ASSERT_TRUE(serialized) << serialized.error();
    auto const valid = nlohmann::json::parse(*serialized);
    struct Case {
        char const* name;
        void (*mutate)(nlohmann::json&);
    };
    for (auto const test :
         {Case{"unknown reason",
               [](auto& json) { json["completion"]["reason"] = "future_reason"; }},
          Case{"misaligned sparse arrays",
               [](auto& json) { json["tick_series"]["active_entities"]["values"].push_back(1); }},
          Case{"non-increasing ticks",
               [](auto& json) { json["tick_series"]["active_entities"]["ticks"] = {0, 0}; }},
          Case{"null flattened count",
               [](auto& json) { json["battle_samples"][0]["alive"][0] = nullptr; }},
          Case{"null optional counter",
               [](auto& json) { json["battle_samples"][0]["active_lasers"] = nullptr; }},
          Case{"fractional optional counter",
               [](auto& json) { json["battle_samples"][0]["active_lasers"] = 0.5; }}}) {
        SCOPED_TRACE(test.name);
        auto invalid = valid;
        test.mutate(invalid);
        EXPECT_FALSE(telemetry::deserialize_report(invalid.dump()));
    }
}

} // namespace ioj::sim::telemetry::tests
