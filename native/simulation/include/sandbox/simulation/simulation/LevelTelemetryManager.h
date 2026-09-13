#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <sandbox/simulation/level_telemetry_current_state.h>
#include <sandbox/simulation/level_telemetry_history_stats.h>

#include "sandbox/simulation/simulation/LevelTelemetrySnapshot.h"

#include <sandbox/core/time_series_data.h>
#include <sandbox/simulation/entities/TestEntityRegistry.h>
#include <sandbox/simulation/simulation/SpatialQueryManager.h>
#include <sandbox/simulation/telemetry/LevelTelemetryBlockHistory.h>
#include <sandbox/simulation/telemetry/LevelTelemetryRunRecord.h>

#include <array>
#include <string>

using FLevelTelemetryCurrentState = ml::simulation::LevelTelemetryCurrentState;
using FLevelTelemetryHistoryStats = ml::simulation::LevelTelemetryHistoryStats;

struct FLevelMissionResult;
struct FSimulationClock;
struct FLevelTelemetryManagerTestAccess;

namespace ml::test_lasers {
struct Simulation;
}

class FLevelTelemetryManager {
  public:
    using tick_type = std::uint64_t;
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
        std::array<double, FSimulationTelemetryPerformanceWindow::system_count> const& systems,
        std::array<double, FSimulationTelemetryPerformanceWindow::phase_count> const& phases);
    void mark_mission_terminal(FLevelMissionResult const& result);
    void finalize_interrupted(ml::simulation::LevelTelemetryRunEndReason reason,
                              std::string world_end_reason);
    void finalize_completed(ml::simulation::LevelTelemetryRunEndReason reason,
                            std::optional<ml::simulation::Team> winning_team = {});
    auto is_run_recording() const noexcept -> bool { return run_recording_; }
    auto detailed_timing_enabled() const noexcept -> bool {
        return run_recording_ && metadata_.detailed_timing;
    }
    auto get_performance_window_count() const -> std::int32_t {
        return performance_windows_.size();
    }
    auto take_finalized_run() -> std::optional<FLevelTelemetryRunRecord>;

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
    auto get_completed_ticks_by_real_time() const noexcept
        -> ml::TimeSeriesData<std::uint64_t> const& {
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
    void finalize_run(ml::simulation::LevelTelemetryRunEndReason reason,
                      bool interrupted,
                      std::string world_end_reason,
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
    ml::TimeSeriesData<std::uint64_t> completed_ticks_by_real_time_{};
    std::vector<FLevelTelemetryBattleSample> battle_samples_{};
    std::vector<FSimulationTelemetryPerformanceWindow> performance_windows_{};
    ActiveEntityCountData active_entity_count_data_{};
    CumulativeKillCountData cumulative_kill_count_data_{};

    double run_started_at_{};
    double last_sampled_time_scale_{};
    std::uint64_t payload_write_count_{};
    bool run_recording_{};
    bool run_finalized_{};
    bool run_record_taken_{};
    bool initialized_{};
    bool has_sampled_state_{};

    double next_battle_sample_seconds_{};
    std::vector<double> frame_samples_{};
    std::vector<double> simulation_tick_samples_{};
    std::array<std::vector<double>, FSimulationTelemetryPerformanceWindow::system_count>
        system_samples_{};
    std::array<std::vector<double>, FSimulationTelemetryPerformanceWindow::phase_count>
        phase_samples_{};
};
