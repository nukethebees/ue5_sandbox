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
}

void FLevelTelemetryManager::tick() {
    check(clock_ != nullptr);
    check(entity_registry_ != nullptr);
    check(lasers_ != nullptr);
    check(spatial_queries_ != nullptr);
    update_current_state();
    sample_series();
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

void FLevelTelemetryManager::sample_series() {
    auto const tick{clock_->get_completed_ticks()};
    auto const add_if_changed{[tick](auto& data, auto const value) {
        if (data.is_empty() || data.last_value() != value) {
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
}

void FLevelTelemetryManager::capture_realtime_sample() {
    if (!run_recording_) {
        return;
    }

    add_realtime_sample(clock_->get_completed_ticks(), FPlatformTime::Seconds());
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
    add_realtime_sample(completed_ticks, monotonic_time);

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
