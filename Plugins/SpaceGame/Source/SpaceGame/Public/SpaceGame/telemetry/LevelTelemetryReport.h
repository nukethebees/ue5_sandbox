#pragma once
#include <SpaceGameSimulation/telemetry/LevelTelemetryRunRecord.h>

enum class ELevelTelemetryTimingSystem : uint8 {
    Player,
    Capitals,
    Fighters,
    Turrets,
    Spinners,
    Lasers,
    Registry,
    SpatialQueries,
    Mission,
    Hud,
    Presentation,
    Telemetry,
    COUNT,
};

struct FLevelTelemetryEnvironment {
    FString project_name{};
    FString project_version{};
    FString engine_version{};
    FString build_version{};
    FString build_configuration{};
    FString execution_mode{};
    FString world_type{};
    FString platform{};
    FString host_architecture{};
    FString operating_system_version{};
    FString operating_system_subversion{};
    FString cpu_vendor{};
    FString cpu_brand{};
    int32 physical_core_count{};
    int32 logical_core_count{};
    FString primary_gpu_brand{};
    uint64 total_physical_memory_bytes{};
};

struct FLevelTelemetryReportMetadata : FLevelTelemetryRunMetadata {
    FLevelTelemetryEnvironment environment{};
    FString launch_state{TEXT("running")};
    FString results_navigation{TEXT("none")};
    bool presentation_enabled{};
    FString presentation_mode{TEXT("simulation_only")};
};

struct FLevelTelemetryPerformanceWindow {
    static constexpr int32 system_count{static_cast<int32>(ELevelTelemetryTimingSystem::COUNT)};
    static constexpr int32 phase_count{static_cast<int32>(ELevelTelemetryTimingPhase::COUNT)};
    double real_elapsed_seconds{};
    uint64 completed_tick{};
    FLevelTelemetryTimingAggregate frame{};
    FLevelTelemetryTimingAggregate game_thread{};
    FLevelTelemetryTimingAggregate render_thread{};
    FLevelTelemetryTimingAggregate gpu{};
    FLevelTelemetryTimingAggregate simulation_tick{};
    TStaticArray<FLevelTelemetryTimingAggregate, system_count> systems{};
    TStaticArray<FLevelTelemetryTimingAggregate, phase_count> phases{};
    TStaticArray<double, phase_count> phase_cpu_share{};
};

struct SPACEGAME_API FLevelTelemetryReport {
    static constexpr int32 schema_version{FLevelTelemetryRunRecord::schema_version};
    FLevelTelemetryReport() = default;
    FLevelTelemetryReport(FLevelTelemetryRunRecord record);

    int32 loaded_schema_version{schema_version};
    FLevelTelemetryReportMetadata metadata;
    FLevelTelemetryRunCompletion completion;
    FLevelTelemetryTickSeries tick_series;
    ml::TimeSeriesData<uint64> completed_ticks_by_real_time;
    TArray<FLevelTelemetryBattleSample> battle_samples;
    TArray<FLevelTelemetryPerformanceWindow> performance_windows;
};

struct FLevelExternalTimingSample {
    int32 window_index{};
    ELevelTelemetryTimingSystem system{};
    double seconds{};
};
SPACEGAME_API void append_external_timings(FLevelTelemetryReport& report,
                                           TConstArrayView<FLevelExternalTimingSample> samples);
