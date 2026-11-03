#pragma once
#include "ioj/sim/telemetry/snapshot.h"
#include <ioj/periodic_timer.h>
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

    /* **************************************** */
    // Construction and lifecycle
    /* **************************************** */
    LevelTelemetryManager(SimClock const& clock,
                          EntityLedger const& entity_ledger,
                          lasers::Sim const& lasers,
                          std::pmr::memory_resource& memory,
                          telemetry::HistoryConfig history_config = {}) noexcept;
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
    void set_run_metadata(telemetry::RunMetadata metadata);
    void finalize(telemetry::RunEndReason reason);
    // Returns empty unless the record awaits extraction.
    auto take_finalized_run(Frequency tick_rate_hz) -> std::optional<telemetry::RunRecord>;

    /* **************************************** */
    // Snapshots and queries
    /* **************************************** */
    auto state() const noexcept -> State { return state_; }
    auto make_snapshot() const -> telemetry::Snapshot;

    auto current_state() const noexcept -> telemetry::CurrentState const& { return current_state_; }
    auto history_stats() const noexcept -> telemetry::HistoryStats;
    auto materialize_tick_series() const -> telemetry::TickSeries;
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
    auto materialize_tick_series(telemetry::HistoryFieldMask fields) const -> telemetry::TickSeries;

    /* **************************************** */
    // State
    /* **************************************** */
    SimClock const& clock_;
    EntityLedger const& entity_ledger_;
    lasers::Sim const& lasers_;
    telemetry::CurrentState current_state_{};
    telemetry::CurrentState last_sampled_state_{};
    telemetry::BlockHistory history_;

    telemetry::RunMetadata metadata_{};
    telemetry::RunCompletion completion_{};
    std::vector<telemetry::BattleSample> battle_samples_{};

    State state_{State::Uninitialised};

    PeriodicTimer battle_sample_timer_{PositiveFinite<double>::make(1.0)};
};

} // namespace ioj::sim
