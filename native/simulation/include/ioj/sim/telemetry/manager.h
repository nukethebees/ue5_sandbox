#pragma once
#include "ioj/sim/telemetry/snapshot.h"
#include <ioj/sim/entities/entity_ledger.h>
#include <ioj/sim/sim_tick.h>
#include <ioj/sim/telemetry/block_history.h>
#include <ioj/sim/telemetry/current_state.h>
#include <ioj/sim/telemetry/history_stats.h>
#include <ioj/sim/telemetry/run_record.h>
#include <ioj/time_series_data.h>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace ioj::sim {

struct SimClock;
struct LevelTelemetryManagerTestAccess;

namespace lasers {
struct Sim;
}

class LevelTelemetryManager {
  public:
    enum class State : std::uint8_t {
        Uninitialised,
        Recording,
        Finalized,
        RecordTaken,
    };

    using tick_type = SimTick;
    using ActiveEntityCountData = LevelTelemetrySnapshot::ActiveEntityCountData;
    using CumulativeKillCountData = LevelTelemetrySnapshot::CumulativeKillCountData;

    /* **************************************** */
    // Construction and lifecycle
    /* **************************************** */
    LevelTelemetryManager(SimClock const& clock,
                          EntityLedger const& entity_ledger,
                          lasers::Sim const& lasers,
                          GameMemory& game_memory,
                          LevelTelemetryHistoryConfig history_config = {}) noexcept;
    LevelTelemetryManager(LevelTelemetryManager const&) = delete;
    LevelTelemetryManager(LevelTelemetryManager&&) = delete;
    auto operator=(LevelTelemetryManager const&) -> LevelTelemetryManager& = delete;
    auto operator=(LevelTelemetryManager&&) -> LevelTelemetryManager& = delete;

    // Requires Uninitialised.
    void initialise();
    void reset();
    void tick();

    /* **************************************** */
    // Run capture and completion
    /* **************************************** */
    // Requires Uninitialised or Recording.
    void set_run_metadata(LevelTelemetryRunMetadata metadata);
    void finalize(LevelTelemetryRunEndReason reason);
    // Returns empty unless the record awaits extraction.
    auto take_finalized_run(Frequency tick_rate_hz) -> std::optional<LevelTelemetryRunRecord>;

    /* **************************************** */
    // Snapshots and queries
    /* **************************************** */
    auto get_state() const noexcept -> State { return state_; }
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
    void sample_battle_state();
    auto append_history_row(tick_type completed_tick) -> telemetry::HistoryRows::View;

    /* **************************************** */
    // State
    /* **************************************** */
    SimClock const& clock_;
    EntityLedger const& entity_ledger_;
    lasers::Sim const& lasers_;
    LevelTelemetryCurrentState current_state_{};
    LevelTelemetryCurrentState last_sampled_state_{};
    LevelTelemetryBlockHistory history_;

    LevelTelemetryRunMetadata metadata_{};
    LevelTelemetryRunCompletion completion_{};
    std::vector<LevelTelemetryBattleSample> battle_samples_{};
    ActiveEntityCountData active_entity_count_data_{};
    CumulativeKillCountData cumulative_kill_count_data_{};

    State state_{State::Uninitialised};

    double next_battle_sample_seconds_{};
};

} // namespace ioj::sim
