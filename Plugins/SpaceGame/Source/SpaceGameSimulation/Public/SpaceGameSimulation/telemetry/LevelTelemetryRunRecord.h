#pragma once

#include <sandbox/simulation/level_telemetry_run_data.h>

#include <SpaceGameSimulation/entities/TestTeam.h>
#include <SpaceGameSimulation/missions/TestMissionFailReason.h>
#include <SpaceGameSimulation/missions/TestMissionMode.h>
#include <SpaceGameSimulation/missions/TestMissionState.h>
#include <SpaceGameSimulation/telemetry/LevelTelemetryRunEndReason.h>

#include <CoreMinimal.h>

struct FLevelTelemetryRunMetadata {
    FString run_id{};
    FString map_name{};
    FName level_id{NAME_None};
    FString level_display_name{};
    FString launched_utc{};
    double tick_rate_hz{};
    double tick_period_seconds{};
    double initial_requested_time_scale{1.0};
    bool stop_when_battle_resolved{};
    FString source_sha256{};
    TOptional<double> requested_duration_seconds{};
    double battle_sample_interval_seconds{1.0};
    double performance_window_seconds{0.25};
    uint32 detailed_timing_tick_stride{16};
    bool detailed_timing{};
};

struct FLevelTelemetryRunCompletion {
    ELevelTelemetryRunEndReason reason{ELevelTelemetryRunEndReason::WorldEnd};
    bool interrupted{true};
    FString completed_utc{};
    FString world_end_reason{};
    TOptional<ETestMissionMode> mission_mode{};
    TOptional<ETestMissionState> mission_state{};
    TOptional<ETestMissionFailReason> mission_fail_reason{};
    TOptional<double> mission_elapsed_seconds{};
    uint64 completed_ticks{};
    double simulated_elapsed_seconds{};
    double wall_elapsed_seconds{};
    TOptional<ETestTeam> winning_team{};
};

using FLevelTelemetryBattleSample = ml::simulation::LevelTelemetryBattleSample;
using ESimulationTelemetryTimingSystem = ml::simulation::SimulationTelemetryTimingSystem;
using ELevelTelemetryTimingPhase = ml::simulation::LevelTelemetryTimingPhase;
using FLevelTelemetryTimingAggregate = ml::simulation::LevelTelemetryTimingAggregate;
using FSimulationTelemetryPerformanceWindow = ml::simulation::SimulationTelemetryPerformanceWindow;
using FLevelTelemetryTickSeries = ml::simulation::LevelTelemetryTickSeries;

struct FLevelTelemetryRunRecord {
    static constexpr int32 schema_version{2};

    int32 loaded_schema_version{schema_version};
    FLevelTelemetryRunMetadata metadata{};
    FLevelTelemetryRunCompletion completion{};
    FLevelTelemetryTickSeries tick_series{};
    ml::TimeSeriesData<uint64> completed_ticks_by_real_time{};
    TArray<FLevelTelemetryBattleSample> battle_samples{};
    TArray<FSimulationTelemetryPerformanceWindow> performance_windows{};
};
