#pragma once

#include "SpaceGameSimulation/simulation/LevelTelemetrySnapshot.h"

#include <SandboxCore/time_series_data.h>
#include <SpaceGameSimulation/entities/TestEntityRegistry.h>
#include <SpaceGameSimulation/simulation/SpatialQueryManager.h>
#include <SpaceGameSimulation/telemetry/LevelTelemetryBlockHistory.h>
#include <SpaceGameSimulation/telemetry/LevelTelemetryRunRecord.h>

#include <HAL/Platform.h>

struct FLevelTelemetryCurrentState {
    using EntityCounts = FTestEntityRegistry::EntityCounts;

    EntityCounts active_entities_by_team_and_type{};
    int32 active_entities{};
    int32 spawned_entities{};
    int32 destroyed_entities{};
    int32 kills{};

    int32 registry_slot_count{};

    int32 active_lasers{};
    int32 lasers_fired{};

    int32 occupied_spatial_cell_count{};
    uint64 grid_rebuild_count{};
    uint64 range_query_count{};
    uint64 line_trace_count{};
    uint64 sweep_trace_count{};
};

struct FLevelTelemetryHistoryStats {
    SIZE_T configured_block_bytes{};
    SIZE_T layout_bytes_per_block{};
    int32 rows_per_block{};
    int32 acquired_block_count{};
    int32 retained_block_count{};
    int32 peak_block_count{};
    int32 total_sample_capacity{};
    SIZE_T total_byte_capacity{};
    int32 used_sample_count{};
    SIZE_T used_payload_bytes{};
    int32 unused_samples_in_final_block{};
    SIZE_T unused_payload_bytes_in_final_block{};
    SIZE_T fixed_layout_overhead_bytes{};
    uint64 payload_write_count{};
};

struct FLevelMissionResult;
struct FSimulationClock;
struct FLevelTelemetryManagerTestAccess;

namespace ml::test_lasers {
struct Simulation;
}

class SPACEGAMESIMULATION_API FLevelTelemetryManager {
  public:
    using tick_type = uint64;
    using ActiveEntityCountData = FLevelTelemetrySnapshot::ActiveEntityCountData;
    using CumulativeKillCountData = FLevelTelemetrySnapshot::CumulativeKillCountData;

    /* **************************************** */
    // Construction and lifecycle
    /* **************************************** */
    FLevelTelemetryManager(FSimulationClock const& clock,
                           FTestEntityRegistry const& entity_registry,
                           ml::test_lasers::Simulation const& lasers,
                           ml::FSpatialQueryManager const& spatial_queries,
                           FGameMemory& game_memory,
                           FLevelTelemetryHistoryConfig history_config = {}) noexcept;
    FLevelTelemetryManager(FLevelTelemetryManager const&) = delete;
    FLevelTelemetryManager(FLevelTelemetryManager&&) = delete;
    auto operator=(FLevelTelemetryManager const&) -> FLevelTelemetryManager& = delete;
    auto operator=(FLevelTelemetryManager&&) -> FLevelTelemetryManager& = delete;

    void initialise();
    void reset();
    void tick();

    /* **************************************** */
    // Run capture and completion
    /* **************************************** */
    void begin_run(FLevelTelemetryRunMetadata metadata);
    void observe_frame(double frame_seconds);
    void capture_realtime_sample();
    void record_simulation_tick_timing(
        double elapsed_seconds,
        TStaticArray<double, FSimulationTelemetryPerformanceWindow::system_count> const& systems,
        TStaticArray<double, FSimulationTelemetryPerformanceWindow::phase_count> const& phases);
    void mark_mission_terminal(FLevelMissionResult const& result);
    void finalize_interrupted(ELevelTelemetryRunEndReason reason, FString world_end_reason);
    void finalize_completed(ELevelTelemetryRunEndReason reason,
                            TOptional<ETestTeam> winning_team = {});
    auto is_run_recording() const noexcept -> bool { return run_recording_; }
    auto detailed_timing_enabled() const noexcept -> bool {
        return run_recording_ && metadata_.detailed_timing;
    }
    auto get_performance_window_count() const -> int32 { return performance_windows_.Num(); }
    auto take_finalized_run() -> TOptional<FLevelTelemetryRunRecord>;

    /* **************************************** */
    // Snapshots and queries
    /* **************************************** */
    auto make_snapshot() const -> FLevelTelemetrySnapshot;

    auto get_active_entity_count_data() const noexcept -> ActiveEntityCountData const& {
        return active_entity_count_data_;
    }
    auto get_cumulative_kill_count_data() const noexcept -> CumulativeKillCountData const& {
        return cumulative_kill_count_data_;
    }
    auto get_completed_ticks_by_real_time() const noexcept -> ml::TimeSeriesData<uint64> const& {
        return completed_ticks_by_real_time_;
    }
    auto get_current_state() const noexcept -> FLevelTelemetryCurrentState const& {
        return current_state_;
    }
    auto get_history_stats() const noexcept -> FLevelTelemetryHistoryStats;
    auto materialize_tick_series() const -> FLevelTelemetryTickSeries;
  private:
    friend struct FLevelTelemetryManagerTestAccess;

    /* **************************************** */
    // Sampling
    /* **************************************** */
    void update_current_state();
    void sample_live_series();
    void sample_series();
    void sample_battle_state(bool force = false);
    auto append_history_row(tick_type completed_tick) -> ml::level_telemetry::FHistoryRowsView;

    /* **************************************** */
    // Performance windows and finalization
    /* **************************************** */
    void close_performance_window(double monotonic_time);
    auto wall_elapsed(double monotonic_time) const -> double;
    void add_realtime_sample(tick_type completed_tick, double monotonic_time);
    void finalize_run(ELevelTelemetryRunEndReason reason,
                      bool interrupted,
                      FString world_end_reason,
                      FLevelMissionResult const* mission_result,
                      double monotonic_time);

    /* **************************************** */
    // State
    /* **************************************** */
    FSimulationClock const& clock_;
    FTestEntityRegistry const& entity_registry_;
    ml::test_lasers::Simulation const& lasers_;
    ml::FSpatialQueryManager const& spatial_queries_;
    FLevelTelemetryCurrentState current_state_{};
    FLevelTelemetryCurrentState last_sampled_state_{};
    FLevelTelemetryBlockHistory history_;

    FLevelTelemetryRunMetadata metadata_{};
    FLevelTelemetryRunCompletion completion_{};
    ml::TimeSeriesData<uint64> completed_ticks_by_real_time_{};
    TArray<FLevelTelemetryBattleSample> battle_samples_{};
    TArray<FSimulationTelemetryPerformanceWindow> performance_windows_{};
    ActiveEntityCountData active_entity_count_data_{};
    CumulativeKillCountData cumulative_kill_count_data_{};

    double run_started_at_{};
    double last_sampled_time_scale_{};
    uint64 payload_write_count_{};
    bool run_recording_{};
    bool run_finalized_{};
    bool run_record_taken_{};
    bool initialized_{};
    bool has_sampled_state_{};

    double next_battle_sample_seconds_{};
    TArray<double> frame_samples_{};
    TArray<double> simulation_tick_samples_{};
    TStaticArray<TArray<double>, FSimulationTelemetryPerformanceWindow::system_count>
        system_samples_{};
    TStaticArray<TArray<double>, FSimulationTelemetryPerformanceWindow::phase_count>
        phase_samples_{};
};
