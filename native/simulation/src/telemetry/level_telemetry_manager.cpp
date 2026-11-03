#include <ioj/sim/telemetry/level_telemetry_manager.h>

#include <ioj/sim/entities/entity_ledger.h>
#include <ioj/sim/lasers/sim.h>
#include <ioj/sim/sim_clock.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <format>
#include <limits>
#include <utility>

namespace ioj::sim {
namespace level_telemetry_detail {
auto utc_now_iso8601() -> std::string {
    auto const now{std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now())};
    return std::format("{:%FT%TZ}", now);
}
auto quantize_ticks(Duration const duration, Frequency const rate) -> std::uint64_t {
    auto const ticks{std::ceil(duration.seconds() * rate.hz())};
    assert(std::isfinite(ticks) && ticks >= 0.0 && ticks < std::ldexp(1.0, 64));
    return static_cast<std::uint64_t>(ticks);
}
}

/* **************************************** */
// Construction and lifecycle
/* **************************************** */
LevelTelemetryManager::LevelTelemetryManager(SimClock const& clock,
                                             EntityLedger const& entity_ledger,
                                             lasers::Sim const& lasers,
                                             std::pmr::memory_resource& memory,
                                             telemetry::RecordingConfig config) noexcept
    : clock_{clock}
    , entity_ledger_{entity_ledger}
    , lasers_{lasers}
    , config_{config}
    , history_{memory} {}
void LevelTelemetryManager::initialise() {
    assert(state_ == State::Uninitialised);
    if (metadata_.launched_utc.empty()) {
        metadata_.launched_utc = level_telemetry_detail::utc_now_iso8601();
    }
    interval_ticks_ = config_.interval_ticks.value_or(
        std::max(std::uint64_t{1},
                 level_telemetry_detail::quantize_ticks(config_.interval, clock_.tick_rate())));
    assert(interval_ticks_ > 0);
    auto const tick{clock_.get_completed_ticks().value()};
    assert(interval_ticks_ <= std::numeric_limits<std::uint64_t>::max() - tick);
    next_sample_tick_ = tick + interval_ticks_;
    state_ = State::Recording;
    record_sample();
}
void LevelTelemetryManager::reset() {
    history_.reset();
    metadata_ = {};
    completion_ = {};
    final_state_ = {};
    state_ = State::Uninitialised;
    interval_ticks_ = 0;
    next_sample_tick_ = 0;
}

/* **************************************** */
// Recording
/* **************************************** */
void LevelTelemetryManager::tick() {
    assert(state_ != State::Uninitialised);
    if (state_ != State::Recording || !config_.enabled) {
        return;
    }
    auto const tick{clock_.get_completed_ticks().value()};
    if (tick < next_sample_tick_) {
        return;
    }
    record_sample();
    auto const remaining{std::numeric_limits<std::uint64_t>::max() - tick};
    next_sample_tick_ = tick + std::min(interval_ticks_, remaining);
}
void LevelTelemetryManager::record_sample() {
    if (!config_.enabled) {
        return;
    }
    history_.append(sample_state());
}
auto LevelTelemetryManager::sample_state() const -> telemetry::BattleSample {
    return {.completed_tick = clock_.get_completed_ticks(),
            .combat = entity_ledger_.combat_telemetry(),
            .alive = entity_ledger_.count_alive_per_team_and_type(),
            .active_lasers = static_cast<std::int32_t>(lasers_.num_instances()),
            .lasers_fired = static_cast<std::int32_t>(lasers_.get_number_spawned())};
}

/* **************************************** */
// Snapshots
/* **************************************** */
auto LevelTelemetryManager::make_snapshot() const -> telemetry::Snapshot {
    assert(state_ != State::Uninitialised);
    assert(clock_.phase() == SimulationPhase::BetweenTicks);
    telemetry::Snapshot snapshot;
    snapshot.elapsed_seconds =
        Duration::make(static_cast<double>(clock_.get_completed_ticks().value()) *
                       clock_.tick_rate().period().seconds());
    snapshot.tick_rate_hz = clock_.tick_rate();
    snapshot.active_entities = entity_ledger_.count_alive();
    snapshot.spawned_entities = entity_ledger_.num_unique_ids_issued();
    snapshot.destroyed_entities = snapshot.spawned_entities - snapshot.active_entities;
    snapshot.kills = entity_ledger_.count_kills();
    snapshot.active_lasers = lasers_.num_instances();
    snapshot.lasers_fired = lasers_.get_number_spawned();
    for (auto const& sample : history_.materialize()) {
        snapshot.active_entity_count_data.add(sample.completed_tick.value(),
                                              telemetry::total_count(sample.alive));
        snapshot.cumulative_kill_count_data.add(
            sample.completed_tick.value(),
            static_cast<std::int32_t>(telemetry::total_count(sample.combat.kills)));
    }
    return snapshot;
}

/* **************************************** */
// Run capture
/* **************************************** */
void LevelTelemetryManager::set_run_metadata(telemetry::RunMetadata metadata) {
    assert(state_ == State::Uninitialised || state_ == State::Recording);
    if (metadata.launched_utc.empty()) {
        metadata.launched_utc = std::move(metadata_.launched_utc);
    }
    metadata_ = std::move(metadata);
}
void LevelTelemetryManager::finalize(telemetry::RunEndReason const reason) {
    if (state_ != State::Recording) {
        return;
    }
    record_sample();
    final_state_ = sample_state();
    completion_.reason = reason;
    completion_.completed_utc = level_telemetry_detail::utc_now_iso8601();
    completion_.completed_ticks = clock_.get_completed_ticks();
    state_ = State::Finalized;
}
auto LevelTelemetryManager::take_finalized_run(Frequency const tick_rate_hz)
    -> std::optional<telemetry::RunRecord> {
    if (state_ != State::Finalized) {
        return std::nullopt;
    }
    state_ = State::RecordTaken;
    return telemetry::RunRecord{.metadata = std::move(metadata_),
                                .tick_rate_hz = tick_rate_hz,
                                .completion = std::move(completion_),
                                .final_state = final_state_,
                                .battle_samples = history_.materialize()};
}
}
