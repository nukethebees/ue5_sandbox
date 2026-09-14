#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <ioj/sim/level_telemetry_current_state.h>
#include <ioj/sim/level_telemetry_history_stats.h>
#include <ioj/sim/sim_tick.h>

#include "ioj/sim/level_telemetry_snapshot.h"

#include <ioj/sim/entity_registry.h>
#include <ioj/sim/spatial_query_manager.h>
#include <ioj/sim/telemetry/level_telemetry_block_history.h>
#include <ioj/sim/telemetry/level_telemetry_run_record.h>
#include <sandbox/core/time_series_data.h>

#include <array>
#include <string>

namespace ioj::sim {

struct LevelMissionResult;
struct SimClock;
struct LevelTelemetryManagerTestAccess;

namespace lasers {
struct Sim;
}

class LevelTelemetryManager {
  public:
    using tick_type = ioj::sim::SimTick;
    using ActiveEntityCountData = LevelTelemetrySnapshot::ActiveEntityCountData;
    using CumulativeKillCountData = LevelTelemetrySnapshot::CumulativeKillCountData;

    /* **************************************** */
    // Construction and lifecycle
    /* **************************************** */
    LevelTelemetryManager(SimClock const& clock,
                          EntityRegistry const& entity_registry,
                          ioj::sim::lasers::Sim const& lasers,
                          ioj::sim::SpatialQueryManager const& spatial_queries,
                          GameMemory& game_memory,
                          LevelTelemetryHistoryConfig history_config = {}) noexcept;
    LevelTelemetryManager(LevelTelemetryManager const&) = delete;
    LevelTelemetryManager(LevelTelemetryManager&&) = delete;
    auto operator=(LevelTelemetryManager const&) -> LevelTelemetryManager& = delete;
    auto operator=(LevelTelemetryManager&&) -> LevelTelemetryManager& = delete;

    void initialise();
    void reset();
    void tick();

    /* **************************************** */
    // Run capture and completion
    /* **************************************** */
    void begin_run(LevelTelemetryRunMetadata metadata);
    void observe_frame(double frame_seconds);
    void capture_realtime_sample();
    void record_simulation_tick_timing(
        double elapsed_seconds,
        std::array<double, SimTelemetryPerformanceWindow::system_count> const& systems,
        std::array<double, SimTelemetryPerformanceWindow::phase_count> const& phases);
    void mark_mission_terminal(LevelMissionResult const& result);
    void finalize_interrupted(ioj::sim::LevelTelemetryRunEndReason reason,
                              std::string world_end_reason);
    void finalize_completed(ioj::sim::LevelTelemetryRunEndReason reason,
                            std::optional<ioj::sim::Team> winning_team = {});
    auto is_run_recording() const noexcept -> bool { return run_recording_; }
    auto detailed_timing_enabled() const noexcept -> bool {
        return run_recording_ && metadata_.detailed_timing;
    }
    auto get_performance_window_count() const -> std::int32_t {
        return static_cast<std::int32_t>(performance_windows_.size());
    }
    auto take_finalized_run() -> std::optional<LevelTelemetryRunRecord>;

    /* **************************************** */
    // Snapshots and queries
    /* **************************************** */
    auto make_snapshot() const -> LevelTelemetrySnapshot;

    auto get_active_entity_count_data() const noexcept -> ActiveEntityCountData const& {
        return active_entity_count_data_;
    }
    auto get_cumulative_kill_count_data() const noexcept -> CumulativeKillCountData const& {
        return cumulative_kill_count_data_;
    }
    auto get_completed_ticks_by_real_time() const noexcept
        -> ml::TimeSeriesData<ioj::sim::SimTick> const& {
        return completed_ticks_by_real_time_;
    }
    auto get_current_state() const noexcept -> LevelTelemetryCurrentState const& {
        return current_state_;
    }
    auto get_history_stats() const noexcept -> LevelTelemetryHistoryStats;
    auto materialize_tick_series() const -> LevelTelemetryTickSeries;
  private:
    friend struct LevelTelemetryManagerTestAccess;

    /* **************************************** */
    // Sampling
    /* **************************************** */
    void update_current_state();
    void sample_live_series();
    void sample_series();
    void sample_battle_state(bool force = false);
    auto append_history_row(tick_type completed_tick) -> ioj::sim::telemetry::HistoryRowsView;

    /* **************************************** */
    // Performance windows and finalization
    /* **************************************** */
    void close_performance_window(double monotonic_time);
    auto wall_elapsed(double monotonic_time) const -> double;
    void add_realtime_sample(tick_type completed_tick, double monotonic_time);
    void finalize_run(ioj::sim::LevelTelemetryRunEndReason reason,
                      bool interrupted,
                      std::string world_end_reason,
                      LevelMissionResult const* mission_result,
                      double monotonic_time);

    /* **************************************** */
    // State
    /* **************************************** */
    SimClock const& clock_;
    EntityRegistry const& entity_registry_;
    ioj::sim::lasers::Sim const& lasers_;
    ioj::sim::SpatialQueryManager const& spatial_queries_;
    LevelTelemetryCurrentState current_state_{};
    LevelTelemetryCurrentState last_sampled_state_{};
    LevelTelemetryBlockHistory history_;

    LevelTelemetryRunMetadata metadata_{};
    LevelTelemetryRunCompletion completion_{};
    ml::TimeSeriesData<ioj::sim::SimTick> completed_ticks_by_real_time_{};
    std::vector<LevelTelemetryBattleSample> battle_samples_{};
    std::vector<SimTelemetryPerformanceWindow> performance_windows_{};
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
    std::array<std::vector<double>, SimTelemetryPerformanceWindow::system_count> system_samples_{};
    std::array<std::vector<double>, SimTelemetryPerformanceWindow::phase_count> phase_samples_{};
};

} // namespace ioj::sim
