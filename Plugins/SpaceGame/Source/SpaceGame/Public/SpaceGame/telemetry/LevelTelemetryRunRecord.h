#pragma once

#include <SandboxCore/time_series_data.h>
#include <SandboxGameShared/utilities/enums.h>
#include <SpaceGame/entities/TestEntityType.h>
#include <SpaceGame/entities/TestTeam.h>
#include <SpaceGame/missions/TestMissionFailReason.h>
#include <SpaceGame/missions/TestMissionMode.h>
#include <SpaceGame/missions/TestMissionState.h>
#include <SpaceGame/telemetry/LevelTelemetryRunEndReason.h>

#include <Containers/StaticArray.h>
#include <CoreMinimal.h>

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

struct FLevelTelemetryRunMetadata {
    FString run_id{};
    FString map_name{};
    FName level_id{NAME_None};
    FString level_display_name{};
    FString launched_utc{};
    FLevelTelemetryEnvironment environment{};
    double tick_rate_hz{};
    double tick_period_seconds{};
    double initial_requested_time_scale{1.0};
    bool presentation_enabled{};
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
};

struct FLevelTelemetryTickSeries {
    static constexpr int32 team_count{ml::EnumCountTrait<ETestTeam>::count_value};
    static constexpr int32 entity_type_count{ml::EnumCountTrait<ETestEntityType>::count_value};

    using Int32Data = ml::XYSeriesData<uint64, int32>;
    using Uint64Data = ml::XYSeriesData<uint64, uint64>;
    using DoubleData = ml::XYSeriesData<uint64, double>;
    using ActiveEntitiesByTypeData = TStaticArray<Int32Data, entity_type_count>;
    using ActiveEntitiesByTeamAndTypeData = TStaticArray<ActiveEntitiesByTypeData, team_count>;

    Int32Data active_entities{};
    ActiveEntitiesByTypeData active_entities_by_type{};
    ActiveEntitiesByTeamAndTypeData active_entities_by_team_and_type{};
    Int32Data spawned_entities{};
    Int32Data destroyed_entities{};
    Int32Data kills{};
    Int32Data registry_slot_count{};
    Int32Data active_lasers{};
    Int32Data lasers_fired{};
    Int32Data occupied_spatial_cell_count{};
    Uint64Data grid_rebuild_count{};
    Uint64Data range_query_count{};
    Uint64Data line_trace_count{};
    Uint64Data sweep_trace_count{};
    DoubleData requested_time_scale{};
};

struct FLevelTelemetryRunRecord {
    static constexpr int32 schema_version{1};

    FLevelTelemetryRunMetadata metadata{};
    FLevelTelemetryRunCompletion completion{};
    FLevelTelemetryTickSeries tick_series{};
    ml::TimeSeriesData<uint64> completed_ticks_by_real_time{};
};
