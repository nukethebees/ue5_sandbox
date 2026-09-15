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
    using tick_type = SimTick;
    using ActiveEntityCountData = LevelTelemetrySnapshot::ActiveEntityCountData;
    using CumulativeKillCountData = LevelTelemetrySnapshot::CumulativeKillCountData;

    /* **************************************** */
    // Construction and lifecycle
    /* **************************************** */
    LevelTelemetryManager(SimClock const& clock,
                          EntityRegistry const& entity_registry,
                          lasers::Sim const& lasers,
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
    void mark_mission_terminal(LevelMissionResult const& result);
    void finalize_interrupted(LevelTelemetryRunEndReason reason, std::string world_end_reason);
    void finalize_completed(LevelTelemetryRunEndReason reason,
                            std::optional<Team> winning_team = {});
    auto is_run_recording() const noexcept -> bool { return run_recording_; }
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
    auto append_history_row(tick_type completed_tick) -> telemetry::HistoryRowsView;

    /* **************************************** */
    // Finalization
    /* **************************************** */
    void finalize_run(LevelTelemetryRunEndReason reason,
                      bool interrupted,
                      std::string world_end_reason,
                      LevelMissionResult const* mission_result);

    /* **************************************** */
    // State
    /* **************************************** */
    SimClock const& clock_;
    EntityRegistry const& entity_registry_;
    lasers::Sim const& lasers_;
    LevelTelemetryCurrentState current_state_{};
    LevelTelemetryCurrentState last_sampled_state_{};
    LevelTelemetryBlockHistory history_;

    LevelTelemetryRunMetadata metadata_{};
    LevelTelemetryRunCompletion completion_{};
    std::vector<LevelTelemetryBattleSample> battle_samples_{};
    ActiveEntityCountData active_entity_count_data_{};
    CumulativeKillCountData cumulative_kill_count_data_{};

    bool run_recording_{};
    bool run_finalized_{};
    bool run_record_taken_{};
    bool initialized_{};
    bool has_sampled_state_{};

    double next_battle_sample_seconds_{};
};

} // namespace ioj::sim
