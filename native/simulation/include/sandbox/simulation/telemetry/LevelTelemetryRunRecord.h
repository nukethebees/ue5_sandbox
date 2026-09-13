#pragma once

#include <sandbox/simulation/level_telemetry_run_data.h>

#include <sandbox/simulation/entity_types.h>
#include <sandbox/simulation/missions/mission_fail_reason.h>
#include <sandbox/simulation/missions/mission_mode.h>
#include <sandbox/simulation/missions/mission_state.h>
#include <sandbox/simulation/sim_tick.h>
#include <sandbox/simulation/telemetry/LevelTelemetryRunEndReason.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct FLevelTelemetryRunMetadata {
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

struct FLevelTelemetryRunCompletion {
    ml::simulation::LevelTelemetryRunEndReason reason{
        ml::simulation::LevelTelemetryRunEndReason::WorldEnd};
    bool interrupted{true};
    std::string completed_utc{};
    std::string world_end_reason{};
    std::optional<ml::simulation::MissionMode> mission_mode{};
    std::optional<ml::simulation::MissionState> mission_state{};
    std::optional<ml::simulation::MissionFailReason> mission_fail_reason{};
    std::optional<double> mission_elapsed_seconds{};
    ml::simulation::SimTick completed_ticks{};
    double simulated_elapsed_seconds{};
    double wall_elapsed_seconds{};
    std::optional<ml::simulation::Team> winning_team{};
};

using FLevelTelemetryBattleSample = ml::simulation::LevelTelemetryBattleSample;
using ESimulationTelemetryTimingSystem = ml::simulation::SimulationTelemetryTimingSystem;
using ELevelTelemetryTimingPhase = ml::simulation::LevelTelemetryTimingPhase;
using FLevelTelemetryTimingAggregate = ml::simulation::LevelTelemetryTimingAggregate;
using FSimulationTelemetryPerformanceWindow = ml::simulation::SimulationTelemetryPerformanceWindow;
using FLevelTelemetryTickSeries = ml::simulation::LevelTelemetryTickSeries;

struct FLevelTelemetryRunRecord {
    static constexpr std::int32_t schema_version{2};

    std::int32_t loaded_schema_version{schema_version};
    FLevelTelemetryRunMetadata metadata{};
    FLevelTelemetryRunCompletion completion{};
    FLevelTelemetryTickSeries tick_series{};
    ml::TimeSeriesData<ml::simulation::SimTick> completed_ticks_by_real_time{};
    std::vector<FLevelTelemetryBattleSample> battle_samples{};
    std::vector<FSimulationTelemetryPerformanceWindow> performance_windows{};
};
