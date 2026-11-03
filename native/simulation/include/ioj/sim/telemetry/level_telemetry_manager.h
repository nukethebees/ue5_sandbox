#pragma once

#include <ioj/sim/telemetry/battle_history.h>
#include <ioj/sim/telemetry/recording_config.h>
#include <ioj/sim/telemetry/run_record.h>
#include <ioj/sim/telemetry/snapshot.h>

#include <optional>

namespace ioj::sim {
class EntityLedger;
struct SimClock;
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

    LevelTelemetryManager(SimClock const& clock,
                          EntityLedger const& entity_ledger,
                          lasers::Sim const& lasers,
                          std::pmr::memory_resource& memory,
                          telemetry::RecordingConfig config = {}) noexcept;
    LevelTelemetryManager(LevelTelemetryManager const&) = delete;
    LevelTelemetryManager(LevelTelemetryManager&&) = delete;
    auto operator=(LevelTelemetryManager const&) -> LevelTelemetryManager& = delete;
    auto operator=(LevelTelemetryManager&&) -> LevelTelemetryManager& = delete;

    void initialise();
    void reset();
    void tick();
    void set_run_metadata(telemetry::RunMetadata metadata);
    void finalize(telemetry::RunEndReason reason);
    auto take_finalized_run(Frequency tick_rate_hz) -> std::optional<telemetry::RunRecord>;

    auto state() const noexcept -> State { return state_; }
    // Read live totals at a completed-tick boundary; graphs end at the last recorded tick.
    auto make_snapshot() const -> telemetry::Snapshot;
    auto materialize_history() const -> std::vector<telemetry::BattleSample> {
        return history_.materialize();
    }
    auto recorded_sample_count() const noexcept -> std::size_t { return history_.size(); }
    auto sampling_interval_ticks() const noexcept -> std::uint64_t { return interval_ticks_; }

  private:
    auto sample_state() const -> telemetry::BattleSample;
    void record_sample();

    SimClock const& clock_;
    EntityLedger const& entity_ledger_;
    lasers::Sim const& lasers_;
    telemetry::RecordingConfig config_;
    telemetry::BattleHistory history_;
    telemetry::RunMetadata metadata_{};
    telemetry::RunCompletion completion_{};
    telemetry::BattleSample final_state_{};
    State state_{State::Uninitialised};
    std::uint64_t interval_ticks_{};
    std::uint64_t next_sample_tick_{};
    std::optional<std::uint64_t> end_tick_;
};
}
