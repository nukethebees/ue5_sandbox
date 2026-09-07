#include "SpaceGame/simulation/LevelTelemetryManager.h"

#include <SpaceGame/combat/lasers/TestLasersSimulation.h>
#include <SpaceGame/missions/TestMissionManager.h>
#include <SpaceGame/simulation/SimulationClock.h>

#include <HAL/PlatformTime.h>
#include <Misc/DateTime.h>

namespace {
auto sum(FTestEntityRegistry::EntityCounts const& entity_counts) -> int32 {
    int32 total{};
    for (auto const& team_counts : entity_counts) {
        for (auto const count : team_counts) {
            total += count;
        }
    }

    return total;
}

template <typename Data>
auto snapshot_with_terminal_sample(Data const& source,
                                   FLevelTelemetryManager::tick_type const completed_tick) -> Data {
    auto result{source};
    if (!result.is_empty() && result.last_time() < completed_tick) {
        auto const terminal_value{result.last_value()};
        result.add(completed_tick, terminal_value);
    }
    return result;
}

template <typename Data>
bool contains_only_nonnegative_values(Data const& data) {
    auto const sample_count{data.num()};
    for (int32 i{0}; i < sample_count; ++i) {
        if (data.value_at(i) < 0) {
            return false;
        }
    }
    return true;
}

auto aggregate_timings(TArray<double> samples) -> FLevelTelemetryTimingAggregate {
    FLevelTelemetryTimingAggregate result;
    result.sample_count = samples.Num();
    if (samples.IsEmpty()) {
        return result;
    }
    double total{};
    for (auto const sample : samples) {
        total += sample;
        result.max_ms = FMath::Max(result.max_ms, sample * 1000.0);
    }
    samples.Sort();
    auto const p95_index{
        FMath::Clamp(FMath::CeilToInt(samples.Num() * 0.95) - 1, 0, samples.Num() - 1)};
    result.mean_ms = total * 1000.0 / samples.Num();
    result.p95_ms = samples[p95_index] * 1000.0;
    return result;
}

}

void FLevelTelemetryManager::initialise(FSimulationClock const& clock,
                                        FTestEntityRegistry const& entity_registry,
                                        ml::test_lasers::Simulation const& lasers,
                                        ml::FSpatialQueryManager const& spatial_queries) {
    reset();
    clock_ = &clock;
    entity_registry_ = &entity_registry;
    lasers_ = &lasers;
    spatial_queries_ = &spatial_queries;
    update_current_state();
    sample_series();
}

void FLevelTelemetryManager::reset() {
    clock_ = nullptr;
    entity_registry_ = nullptr;
    lasers_ = nullptr;
    spatial_queries_ = nullptr;
    current_state_ = {};
    run_record_ = {};
    run_started_at_ = 0.0;
    run_recording_ = false;
    run_finalized_ = false;
    run_record_taken_ = false;
    next_battle_sample_seconds_ = 0.0;
    frame_samples_.Reset();
    simulation_tick_samples_.Reset();
    for (auto& samples : system_samples_) {
        samples.Reset();
    }
    for (auto& samples : phase_samples_) {
        samples.Reset();
    }
}

void FLevelTelemetryManager::tick() {
    check(clock_ != nullptr);
    check(entity_registry_ != nullptr);
    check(lasers_ != nullptr);
    check(spatial_queries_ != nullptr);
    update_current_state();
    auto& requested_time_scale{run_record_.tick_series.requested_time_scale};
    auto const completed_tick{clock_->get_completed_ticks()};
    auto const time_scale{clock_->get_time_scale()};
    if (requested_time_scale.is_empty() || requested_time_scale.last_value() != time_scale) {
        requested_time_scale.add(completed_tick, time_scale);
    }
    auto const simulated_seconds{clock_->get_simulation_time()};
    if (simulated_seconds + UE_DOUBLE_SMALL_NUMBER >= next_battle_sample_seconds_) {
        sample_series();
        sample_battle_state();
        next_battle_sample_seconds_ = FMath::FloorToDouble(simulated_seconds) + 1.0;
    }
}

void FLevelTelemetryManager::observe_frame(double const frame_seconds) {
    if (run_recording_ && frame_seconds >= 0.0) {
        frame_samples_.Add(frame_seconds);
    }
}

void FLevelTelemetryManager::record_simulation_tick_timing(
    double const elapsed_seconds,
    TStaticArray<double, FLevelTelemetryPerformanceWindow::system_count> const& systems,
    TStaticArray<double, FLevelTelemetryPerformanceWindow::phase_count> const& phases) {
    if (!run_recording_ || !run_record_.metadata.detailed_timing) {
        return;
    }
    simulation_tick_samples_.Add(elapsed_seconds);
    for (int32 index{}; index < FLevelTelemetryPerformanceWindow::system_count; ++index) {
        if (systems[index] >= 0.0) {
            system_samples_[index].Add(systems[index]);
        }
    }
    for (int32 index{}; index < FLevelTelemetryPerformanceWindow::phase_count; ++index) {
        if (phases[index] >= 0.0) {
            phase_samples_[index].Add(phases[index]);
        }
    }
}

void FLevelTelemetryManager::record_external_timing(ELevelTelemetryTimingSystem const system,
                                                    double const elapsed_seconds) {
    if (!detailed_timing_enabled()) {
        return;
    }
    system_samples_[static_cast<int32>(system)].Add(elapsed_seconds);
}

auto FLevelTelemetryManager::make_snapshot() const -> FLevelTelemetrySnapshot {
    check(clock_ != nullptr);
    auto const completed_tick{clock_->get_completed_ticks()};
    auto const tick_period{clock_->get_tick_period()};
    checkf(FMath::IsFinite(tick_period) && tick_period > 0.0,
           TEXT("Level telemetry snapshots require a finite, positive tick period."));
    auto const& active_entity_count_data{run_record_.tick_series.active_entities};
    auto const& cumulative_kill_count_data{run_record_.tick_series.kills};
    checkf(!active_entity_count_data.is_empty() && !cumulative_kill_count_data.is_empty(),
           TEXT("Level telemetry must be initialised before creating a snapshot."));
    checkf(active_entity_count_data.last_time() <= completed_tick &&
               cumulative_kill_count_data.last_time() <= completed_tick,
           TEXT("Level telemetry cannot contain samples after the snapshot tick."));
    checkf(contains_only_nonnegative_values(active_entity_count_data) &&
               contains_only_nonnegative_values(cumulative_kill_count_data) &&
               current_state_.spawned_entities >= 0 && current_state_.active_lasers >= 0 &&
               current_state_.lasers_fired >= 0,
           TEXT("Level telemetry snapshots cannot contain negative counts."));

    FLevelTelemetrySnapshot snapshot;
    snapshot.elapsed_seconds = static_cast<double>(completed_tick) * tick_period;
    snapshot.tick_period = tick_period;
    snapshot.active_entities = current_state_.active_entities;
    snapshot.spawned_entities = current_state_.spawned_entities;
    checkf(snapshot.spawned_entities >= snapshot.active_entities,
           TEXT("Spawned entity count cannot be lower than active entity count."));
    snapshot.destroyed_entities = current_state_.destroyed_entities;
    snapshot.kills = current_state_.kills;
    snapshot.active_lasers = current_state_.active_lasers;
    snapshot.lasers_fired = current_state_.lasers_fired;
    snapshot.active_entity_count_data =
        snapshot_with_terminal_sample(active_entity_count_data, completed_tick);
    snapshot.cumulative_kill_count_data =
        snapshot_with_terminal_sample(cumulative_kill_count_data, completed_tick);
    return snapshot;
}

void FLevelTelemetryManager::update_current_state() {
    auto const spatial{spatial_queries_->get_runtime_telemetry()};
    current_state_.active_entities_by_team_and_type =
        entity_registry_->count_alive_per_team_and_type();
    current_state_.active_entities = sum(current_state_.active_entities_by_team_and_type);
    current_state_.spawned_entities = entity_registry_->get_num_unique_ids_issued();
    current_state_.destroyed_entities =
        current_state_.spawned_entities - current_state_.active_entities;
    current_state_.kills = entity_registry_->count_kills();
    current_state_.registry_slot_count = entity_registry_->get_num_elements();
    current_state_.active_lasers = lasers_->get_num_instances();
    current_state_.lasers_fired = lasers_->get_number_spawned();
    current_state_.occupied_spatial_cell_count = spatial.occupied_dynamic_cell_count;
    current_state_.grid_rebuild_count = spatial.grid_rebuild_count;
    current_state_.range_query_count = spatial.range_query_count;
    current_state_.line_trace_count = spatial.line_trace_count;
    current_state_.sweep_trace_count = spatial.sweep_trace_count;

    check(current_state_.destroyed_entities >= 0);
}

void FLevelTelemetryManager::sample_series(bool const force) {
    auto const tick{clock_->get_completed_ticks()};
    auto const add_if_changed{[tick, force](auto& data, auto const value) {
        if (data.is_empty() || data.last_value() != value || (force && data.last_time() != tick)) {
            data.add(tick, value);
        }
    }};
    auto& series{run_record_.tick_series};
    add_if_changed(series.active_entities, current_state_.active_entities);

    constexpr auto team_count{FLevelTelemetryTickSeries::team_count};
    constexpr auto entity_type_count{FLevelTelemetryTickSeries::entity_type_count};
    for (int32 entity_type_index{}; entity_type_index < entity_type_count; ++entity_type_index) {
        int32 type_total{};
        for (int32 team_index{}; team_index < team_count; ++team_index) {
            auto const count{
                current_state_.active_entities_by_team_and_type[team_index][entity_type_index]};
            type_total += count;
            add_if_changed(series.active_entities_by_team_and_type[team_index][entity_type_index],
                           count);
        }
        add_if_changed(series.active_entities_by_type[entity_type_index], type_total);
    }

    add_if_changed(series.spawned_entities, current_state_.spawned_entities);
    add_if_changed(series.destroyed_entities, current_state_.destroyed_entities);
    add_if_changed(series.kills, current_state_.kills);
    add_if_changed(series.registry_slot_count, current_state_.registry_slot_count);
    add_if_changed(series.active_lasers, current_state_.active_lasers);
    add_if_changed(series.lasers_fired, current_state_.lasers_fired);
    add_if_changed(series.occupied_spatial_cell_count, current_state_.occupied_spatial_cell_count);
    add_if_changed(series.grid_rebuild_count, current_state_.grid_rebuild_count);
    add_if_changed(series.range_query_count, current_state_.range_query_count);
    add_if_changed(series.line_trace_count, current_state_.line_trace_count);
    add_if_changed(series.sweep_trace_count, current_state_.sweep_trace_count);
    add_if_changed(series.requested_time_scale, clock_->get_time_scale());
}

void FLevelTelemetryManager::sample_battle_state(bool const force) {
    auto const tick{clock_->get_completed_ticks()};
    auto& samples{run_record_.battle_samples};
    if (!force && !samples.IsEmpty() && samples.Last().completed_tick == tick) {
        return;
    }
    if (force && !samples.IsEmpty() && samples.Last().completed_tick == tick) {
        samples.Pop(EAllowShrinking::No);
    }
    samples.Add(FLevelTelemetryBattleSample{
        .completed_tick = tick,
        .simulated_elapsed_seconds = clock_->get_simulation_time(),
        .combat = entity_registry_->get_combat_telemetry(),
        .alive = current_state_.active_entities_by_team_and_type,
        .active_lasers = current_state_.active_lasers,
        .lasers_fired = current_state_.lasers_fired,
        .registry_slot_count = current_state_.registry_slot_count,
        .occupied_spatial_cell_count = current_state_.occupied_spatial_cell_count,
        .grid_rebuild_count = current_state_.grid_rebuild_count,
        .range_query_count = current_state_.range_query_count,
        .line_trace_count = current_state_.line_trace_count,
        .sweep_trace_count = current_state_.sweep_trace_count,
    });
}

void FLevelTelemetryManager::begin_run(FLevelTelemetryRunMetadata metadata) {
    check(clock_ != nullptr);
    check(clock_->get_tick_rate() > 0.0);
    check(clock_->get_tick_period() > 0.0);
    check(clock_->get_time_scale() > 0.0);
    check(!run_record_.tick_series.active_entities.is_empty());
    check(!run_record_.tick_series.requested_time_scale.is_empty());

    metadata.tick_rate_hz = clock_->get_tick_rate();
    metadata.tick_period_seconds = clock_->get_tick_period();
    metadata.initial_requested_time_scale = clock_->get_time_scale();
    run_record_.metadata = MoveTemp(metadata);
    run_record_.completion = {};
    run_record_.completed_ticks_by_real_time.reset();
    run_started_at_ = FPlatformTime::Seconds();
    run_recording_ = true;
    run_finalized_ = false;
    run_record_taken_ = false;
    add_realtime_sample(clock_->get_completed_ticks(), run_started_at_);
    sample_battle_state(true);
    next_battle_sample_seconds_ = 1.0;
}

void FLevelTelemetryManager::capture_realtime_sample() {
    if (!run_recording_) {
        return;
    }

    auto const now{FPlatformTime::Seconds()};
    add_realtime_sample(clock_->get_completed_ticks(), now);
    close_performance_window(now);
}

void FLevelTelemetryManager::close_performance_window(double const monotonic_time) {
    FLevelTelemetryPerformanceWindow window;
    window.real_elapsed_seconds = wall_elapsed(monotonic_time);
    window.completed_tick = clock_->get_completed_ticks();
    window.frame = aggregate_timings(MoveTemp(frame_samples_));
    window.game_thread = window.frame;
    window.simulation_tick = aggregate_timings(MoveTemp(simulation_tick_samples_));
    for (int32 index{}; index < FLevelTelemetryPerformanceWindow::system_count; ++index) {
        window.systems[index] = aggregate_timings(MoveTemp(system_samples_[index]));
        system_samples_[index].Reset();
    }
    double phase_total_ms{};
    for (int32 index{}; index < FLevelTelemetryPerformanceWindow::phase_count; ++index) {
        window.phases[index] = aggregate_timings(MoveTemp(phase_samples_[index]));
        phase_total_ms += window.phases[index].mean_ms;
        phase_samples_[index].Reset();
    }
    if (phase_total_ms > 0.0) {
        for (int32 index{}; index < FLevelTelemetryPerformanceWindow::phase_count; ++index) {
            window.phase_cpu_share[index] = window.phases[index].mean_ms / phase_total_ms;
        }
    }
    run_record_.performance_windows.Add(MoveTemp(window));
    frame_samples_.Reset();
    simulation_tick_samples_.Reset();
}

void FLevelTelemetryManager::mark_mission_terminal(FLevelMissionResult const& result) {
    auto const reason{result.state == ETestMissionState::Succeeded
                          ? ELevelTelemetryRunEndReason::MissionSucceeded
                          : ELevelTelemetryRunEndReason::MissionFailed};
    finalize_run(reason, false, {}, &result, FPlatformTime::Seconds());
}

void FLevelTelemetryManager::finalize_interrupted(ELevelTelemetryRunEndReason const reason,
                                                  FString world_end_reason) {
    finalize_run(reason, true, MoveTemp(world_end_reason), nullptr, FPlatformTime::Seconds());
}

void FLevelTelemetryManager::finalize_completed(ELevelTelemetryRunEndReason const reason,
                                                TOptional<ETestTeam> winning_team) {
    finalize_run(reason, false, {}, nullptr, FPlatformTime::Seconds());
    run_record_.completion.winning_team = winning_team;
}

auto FLevelTelemetryManager::take_finalized_run() -> TOptional<FLevelTelemetryRunRecord> {
    if (!run_finalized_ || run_record_taken_) {
        return NullOpt;
    }

    run_record_taken_ = true;
    return MoveTemp(run_record_);
}

auto FLevelTelemetryManager::wall_elapsed(double const monotonic_time) const -> double {
    return FMath::Max(0.0, monotonic_time - run_started_at_);
}

void FLevelTelemetryManager::add_realtime_sample(tick_type const completed_tick,
                                                 double const monotonic_time) {
    auto& data{run_record_.completed_ticks_by_real_time};
    auto const elapsed{wall_elapsed(monotonic_time)};
    if (data.is_empty() || data.last_time() < elapsed) {
        data.add(elapsed, completed_tick);
    }
}

void FLevelTelemetryManager::finalize_run(ELevelTelemetryRunEndReason const reason,
                                          bool const interrupted,
                                          FString world_end_reason,
                                          FLevelMissionResult const* const mission_result,
                                          double const monotonic_time) {
    if (!run_recording_ || run_finalized_) {
        return;
    }

    auto const completed_ticks{clock_->get_completed_ticks()};
    update_current_state();
    sample_series();
    sample_battle_state(true);
    add_realtime_sample(completed_ticks, monotonic_time);
    if (!frame_samples_.IsEmpty() || !simulation_tick_samples_.IsEmpty()) {
        close_performance_window(monotonic_time);
    }

    run_record_.completion.reason = reason;
    run_record_.completion.interrupted = interrupted;
    run_record_.completion.completed_utc = FDateTime::UtcNow().ToIso8601();
    run_record_.completion.world_end_reason = MoveTemp(world_end_reason);
    run_record_.completion.completed_ticks = completed_ticks;
    run_record_.completion.simulated_elapsed_seconds = clock_->get_simulation_time();
    run_record_.completion.wall_elapsed_seconds = wall_elapsed(monotonic_time);
    if (mission_result != nullptr) {
        run_record_.completion.mission_mode = mission_result->mode;
        run_record_.completion.mission_state = mission_result->state;
        run_record_.completion.mission_fail_reason = mission_result->fail_reason;
        run_record_.completion.mission_elapsed_seconds = mission_result->elapsed_seconds;
    }

    run_recording_ = false;
    run_finalized_ = true;
}
