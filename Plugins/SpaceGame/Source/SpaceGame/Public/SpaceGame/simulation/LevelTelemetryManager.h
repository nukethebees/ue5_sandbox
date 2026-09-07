#pragma once

#include "SpaceGame/simulation/LevelTelemetrySnapshot.h"

#include <SandboxCore/time_series_data.h>
#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/simulation/SpatialQueryManager.h>
#include <SpaceGame/telemetry/LevelTelemetryRunRecord.h>

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

struct FLevelMissionResult;
struct FSimulationClock;

namespace ml::test_lasers {
struct Simulation;
}

class SPACEGAME_API FLevelTelemetryManager {
  public:
    using tick_type = uint64;
    using ActiveEntityCountData = FLevelTelemetrySnapshot::ActiveEntityCountData;
    using CumulativeKillCountData = FLevelTelemetrySnapshot::CumulativeKillCountData;

    void initialise(FSimulationClock const& clock,
                    FTestEntityRegistry const& entity_registry,
                    ml::test_lasers::Simulation const& lasers,
                    ml::FSpatialQueryManager const& spatial_queries);
    void reset();
    void tick();

    void begin_run(FLevelTelemetryRunMetadata metadata);
    void observe_frame(double frame_seconds);
    void capture_realtime_sample();
    void record_simulation_tick_timing(
        double elapsed_seconds,
        TStaticArray<double, FLevelTelemetryPerformanceWindow::system_count> const& systems,
        TStaticArray<double, FLevelTelemetryPerformanceWindow::phase_count> const& phases);
    void record_external_timing(ELevelTelemetryTimingSystem system, double elapsed_seconds);
    void mark_mission_terminal(FLevelMissionResult const& result);
    void finalize_interrupted(ELevelTelemetryRunEndReason reason, FString world_end_reason);
    void finalize_completed(ELevelTelemetryRunEndReason reason,
                            TOptional<ETestTeam> winning_team = {});
    auto is_run_recording() const noexcept -> bool { return run_recording_; }
    auto detailed_timing_enabled() const noexcept -> bool {
        return run_recording_ && run_record_.metadata.detailed_timing;
    }
    auto take_finalized_run() -> TOptional<FLevelTelemetryRunRecord>;

    auto make_snapshot() const -> FLevelTelemetrySnapshot;

    auto get_active_entity_count_data() const noexcept -> ActiveEntityCountData const& {
        return run_record_.tick_series.active_entities;
    }
    auto get_cumulative_kill_count_data() const noexcept -> CumulativeKillCountData const& {
        return run_record_.tick_series.kills;
    }
    auto get_tick_series() const noexcept -> FLevelTelemetryTickSeries const& {
        return run_record_.tick_series;
    }
    auto get_completed_ticks_by_real_time() const noexcept -> ml::TimeSeriesData<uint64> const& {
        return run_record_.completed_ticks_by_real_time;
    }
    auto get_current_state() const noexcept -> FLevelTelemetryCurrentState const& {
        return current_state_;
    }
  private:
    void update_current_state();
    void sample_series(bool force = false);
    void sample_battle_state(bool force = false);
    void close_performance_window(double monotonic_time);
    auto wall_elapsed(double monotonic_time) const -> double;
    void add_realtime_sample(tick_type completed_tick, double monotonic_time);
    void finalize_run(ELevelTelemetryRunEndReason reason,
                      bool interrupted,
                      FString world_end_reason,
                      FLevelMissionResult const* mission_result,
                      double monotonic_time);

    FSimulationClock const* clock_{};
    FTestEntityRegistry const* entity_registry_{};
    ml::test_lasers::Simulation const* lasers_{};
    ml::FSpatialQueryManager const* spatial_queries_{};
    FLevelTelemetryCurrentState current_state_{};
    FLevelTelemetryRunRecord run_record_{};
    double run_started_at_{};
    bool run_recording_{};
    bool run_finalized_{};
    bool run_record_taken_{};
    double next_battle_sample_seconds_{};
    TArray<double> frame_samples_{};
    TArray<double> simulation_tick_samples_{};
    TStaticArray<TArray<double>, FLevelTelemetryPerformanceWindow::system_count> system_samples_{};
    TStaticArray<TArray<double>, FLevelTelemetryPerformanceWindow::phase_count> phase_samples_{};
};
