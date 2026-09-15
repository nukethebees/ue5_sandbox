#pragma once

#include <ioj/sim/level_telemetry_run_data.h>

#include <ioj/sim/entity_types.h>
#include <ioj/sim/missions/mission_fail_reason.h>
#include <ioj/sim/missions/mission_mode.h>
#include <ioj/sim/missions/mission_state.h>
#include <ioj/sim/sim_tick.h>
#include <ioj/sim/telemetry/level_telemetry_run_end_reason.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ioj::sim {

struct LevelTelemetryRunMetadata {
    std::string run_id{};
    std::string map_name{};
    std::string level_id{};
    std::string level_display_name{};
    std::string launched_utc{};
    double tick_rate_hz{};
    double tick_period_seconds{};
    double initial_requested_time_scale{1.0};
    bool stop_when_battle_resolved{};
    std::string source_sha256{};
    std::optional<double> requested_duration_seconds{};
    double battle_sample_interval_seconds{1.0};
    double performance_window_seconds{0.25};
    std::uint32_t detailed_timing_tick_stride{16};
    bool detailed_timing{};
};

struct LevelTelemetryRunCompletion {
    LevelTelemetryRunEndReason reason{LevelTelemetryRunEndReason::WorldEnd};
    bool interrupted{true};
    std::string completed_utc{};
    std::string world_end_reason{};
    std::optional<MissionMode> mission_mode{};
    std::optional<MissionState> mission_state{};
    std::optional<MissionFailReason> mission_fail_reason{};
    std::optional<double> mission_elapsed_seconds{};
    SimTick completed_ticks{};
    double simulated_elapsed_seconds{};
    double wall_elapsed_seconds{};
    std::optional<Team> winning_team{};
};

struct LevelTelemetryRunRecord {
    static constexpr std::int32_t schema_version{3};

    std::int32_t loaded_schema_version{schema_version};
    LevelTelemetryRunMetadata metadata{};
    LevelTelemetryRunCompletion completion{};
    LevelTelemetryTickSeries tick_series{};
    ml::TimeSeriesData<SimTick> completed_ticks_by_real_time{};
    std::vector<LevelTelemetryBattleSample> battle_samples{};
    std::vector<SimTelemetryPerformanceWindow> performance_windows{};
};
} // namespace ioj::sim
