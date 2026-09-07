#pragma once

#include <SandboxCore/time_series_data.h>
#include <SandboxGameShared/utilities/enums.h>
#include <SpaceGame/entities/TestEntityRegistry.h>
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
    FString launch_state{TEXT("running")};
    FString presentation_mode{TEXT("visual")};
    bool stop_when_battle_resolved{};
    FString results_navigation{TEXT("none")};
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

struct FLevelTelemetryBattleSample {
    uint64 completed_tick{};
    double simulated_elapsed_seconds{};
    FTestEntityRegistry::CombatTelemetryCounters combat{};
    FTestEntityRegistry::EntityCounts alive{};
    int32 active_lasers{};
    int32 lasers_fired{};
    int32 registry_slot_count{};
    int32 occupied_spatial_cell_count{};
    uint64 grid_rebuild_count{};
    uint64 range_query_count{};
    uint64 line_trace_count{};
    uint64 sweep_trace_count{};
};

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

enum class ELevelTelemetryTimingPhase : uint8 {
    Setup,
    Decision,
    Simulation,
    Resolution,
    End,
    COUNT,
};

struct FLevelTelemetryTimingAggregate {
    double mean_ms{};
    double p95_ms{};
    double max_ms{};
    uint64 sample_count{};
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
    static constexpr int32 schema_version{2};

    int32 loaded_schema_version{schema_version};
    FLevelTelemetryRunMetadata metadata{};
    FLevelTelemetryRunCompletion completion{};
    FLevelTelemetryTickSeries tick_series{};
    ml::TimeSeriesData<uint64> completed_ticks_by_real_time{};
    TArray<FLevelTelemetryBattleSample> battle_samples{};
    TArray<FLevelTelemetryPerformanceWindow> performance_windows{};
};
