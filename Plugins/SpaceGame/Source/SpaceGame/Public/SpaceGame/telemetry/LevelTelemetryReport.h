#pragma once
#include <CoreMinimal.h>
#include <ioj/sim/telemetry/level_telemetry_run_record.h>

#include <array>
#include <optional>

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

struct FLevelTelemetryReportMetadata : ::ioj::sim::LevelTelemetryRunMetadata {
    FLevelTelemetryEnvironment environment{};
    FString launch_state{TEXT("running")};
    FString results_navigation{TEXT("none")};
    bool presentation_enabled{};
    FString presentation_mode{TEXT("simulation_only")};
};

struct SPACEGAME_API FLevelTelemetryReport {
    static constexpr int32 schema_version{::ioj::sim::LevelTelemetryRunRecord::schema_version};
    FLevelTelemetryReport() = default;
    FLevelTelemetryReport(::ioj::sim::LevelTelemetryRunRecord record);

    int32 loaded_schema_version{schema_version};
    FLevelTelemetryReportMetadata metadata;
    ::ioj::sim::LevelTelemetryRunCompletion completion;
    ::ioj::sim::LevelTelemetryTickSeries tick_series;
    TArray<::ioj::sim::LevelTelemetryBattleSample> battle_samples;
};
