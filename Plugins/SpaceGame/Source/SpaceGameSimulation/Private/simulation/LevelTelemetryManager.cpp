#include "SpaceGameSimulation/simulation/LevelTelemetryManager.h"

#include <SpaceGameSimulation/combat/lasers/TestLasersSimulation.h>
#include <SpaceGameSimulation/missions/TestMissionManager.h>
#include <SpaceGameSimulation/simulation/SimulationClock.h>

#include <HAL/PlatformTime.h>
#include <Misc/DateTime.h>

#include <bit>

namespace level_telemetry_detail {
using Field = ml::level_telemetry::EHistoryField;
using FieldMask = ml::level_telemetry::FHistoryFieldMask;

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

using level_telemetry_detail::aggregate_timings;
using level_telemetry_detail::contains_only_nonnegative_values;
using level_telemetry_detail::snapshot_with_terminal_sample;
using level_telemetry_detail::sum;

/* **************************************** */
// Construction and lifecycle
/* **************************************** */
FLevelTelemetryManager::FLevelTelemetryManager(FSimulationClock const& clock,
                                               FTestEntityRegistry const& entity_registry,
                                               ml::test_lasers::Simulation const& lasers,
                                               ml::FSpatialQueryManager const& spatial_queries,
                                               FGameMemory& game_memory,
                                               FLevelTelemetryHistoryConfig history_config) noexcept
    : clock_{clock}
    , entity_registry_{entity_registry}
    , lasers_{lasers}
    , spatial_queries_{spatial_queries}
    , history_{game_memory, history_config} {}

void FLevelTelemetryManager::initialise() {
    reset();
    initialized_ = true;
    update_current_state();
    sample_series();
}

void FLevelTelemetryManager::reset() {
    current_state_ = {};
    last_sampled_state_ = {};
    history_.reset();
    metadata_ = {};
    completion_ = {};
    completed_ticks_by_real_time_.reset();
    battle_samples_.Reset();
    performance_windows_.Reset();
    active_entity_count_data_.reset();
    cumulative_kill_count_data_.reset();
    run_started_at_ = 0.0;
    last_sampled_time_scale_ = 0.0;
    payload_write_count_ = 0;
    run_recording_ = false;
    run_finalized_ = false;
    run_record_taken_ = false;
    initialized_ = false;
    has_sampled_state_ = false;
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

/* **************************************** */
// Tick sampling
/* **************************************** */
void FLevelTelemetryManager::tick() {
    check(initialized_);
    update_current_state();
    sample_live_series();

    auto const simulated_seconds{clock_.get_simulation_time()};
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
    TStaticArray<double, FSimulationTelemetryPerformanceWindow::system_count> const& systems,
    TStaticArray<double, FSimulationTelemetryPerformanceWindow::phase_count> const& phases) {
    if (!run_recording_ || !metadata_.detailed_timing) {
        return;
    }

    simulation_tick_samples_.Add(elapsed_seconds);
    for (int32 index{}; index < FSimulationTelemetryPerformanceWindow::system_count; ++index) {
        if (systems[index] >= 0.0) {
            system_samples_[index].Add(systems[index]);
        }
    }

    for (int32 index{}; index < FSimulationTelemetryPerformanceWindow::phase_count; ++index) {
        if (phases[index] >= 0.0) {
            phase_samples_[index].Add(phases[index]);
        }
    }
}

/* **************************************** */
// Snapshots
/* **************************************** */
auto FLevelTelemetryManager::make_snapshot() const -> FLevelTelemetrySnapshot {
    check(initialized_);
    auto const completed_tick{clock_.get_completed_ticks()};
    auto const tick_period{clock_.get_tick_period()};

    checkf(FMath::IsFinite(tick_period) && tick_period > 0.0,
           TEXT("Level telemetry snapshots require a finite, positive tick period."));
    auto const& active_entity_count_data{active_entity_count_data_};
    auto const& cumulative_kill_count_data{cumulative_kill_count_data_};
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

/* **************************************** */
// State and series sampling
/* **************************************** */
void FLevelTelemetryManager::update_current_state() {
    auto const spatial{spatial_queries_.get_runtime_telemetry()};
    current_state_.active_entities_by_team_and_type =
        entity_registry_.count_alive_per_team_and_type();
    current_state_.active_entities = sum(current_state_.active_entities_by_team_and_type);
    current_state_.spawned_entities = entity_registry_.get_num_unique_ids_issued();
    current_state_.destroyed_entities =
        current_state_.spawned_entities - current_state_.active_entities;
    current_state_.kills = entity_registry_.count_kills();
    current_state_.registry_slot_count = entity_registry_.get_num_elements();
    current_state_.active_lasers = lasers_.get_num_instances();
    current_state_.lasers_fired = lasers_.get_number_spawned();
    current_state_.occupied_spatial_cell_count = spatial.occupied_dynamic_cell_count;
    current_state_.grid_rebuild_count = spatial.grid_rebuild_count;
    current_state_.range_query_count = spatial.range_query_count;
    current_state_.line_trace_count = spatial.line_trace_count;
    current_state_.sweep_trace_count = spatial.sweep_trace_count;

    check(current_state_.destroyed_entities >= 0);
}

void FLevelTelemetryManager::sample_live_series() {
    using namespace level_telemetry_detail;
    constexpr auto team_count{FLevelTelemetryTickSeries::team_count};
    constexpr auto entity_type_count{FLevelTelemetryTickSeries::entity_type_count};
    FTestEntityRegistry::EntityTypeCounts active_entities_by_type{};
    FTestEntityRegistry::EntityTypeCounts last_active_entities_by_type{};
    FieldMask mask;

    if (!has_sampled_state_ ||
        current_state_.active_entities != last_sampled_state_.active_entities) {
        mask.set(Field::ActiveEntities);
    }

    for (int32 entity_type_index{}; entity_type_index < entity_type_count; ++entity_type_index) {
        for (int32 team_index{}; team_index < team_count; ++team_index) {
            auto const value{
                current_state_.active_entities_by_team_and_type[team_index][entity_type_index]};
            auto const last_value{
                last_sampled_state_
                    .active_entities_by_team_and_type[team_index][entity_type_index]};
            active_entities_by_type[entity_type_index] += value;
            last_active_entities_by_type[entity_type_index] += last_value;
            if (!has_sampled_state_ || value != last_value) {
                mask.set(FieldMask::active_entities_by_team_and_type_field(team_index,
                                                                           entity_type_index));
            }
        }
        if (!has_sampled_state_ || active_entities_by_type[entity_type_index] !=
                                       last_active_entities_by_type[entity_type_index]) {
            mask.set(FieldMask::active_entities_by_type_field(entity_type_index));
        }
    }

    if (!has_sampled_state_ || current_state_.kills != last_sampled_state_.kills) {
        mask.set(Field::Kills);
    }
    auto const time_scale{clock_.get_time_scale()};
    if (!has_sampled_state_ || time_scale != last_sampled_time_scale_) {
        mask.set(Field::RequestedTimeScale);
    }
    if (mask.is_empty()) {
        return;
    }

    auto const tick{clock_.get_completed_ticks()};
    auto columns{append_history_row(tick)};
    constexpr int32 row{};
    columns.validity_masks[row].set(mask);

    if (mask.has(Field::ActiveEntities)) {
        columns.active_entities[row] = current_state_.active_entities;
        active_entity_count_data_.add(tick, current_state_.active_entities);
    }
    for (int32 entity_type_index{}; entity_type_index < entity_type_count; ++entity_type_index) {
        if (mask.has(FieldMask::active_entities_by_type_field(entity_type_index))) {
            columns.active_entities_by_type[row][entity_type_index] =
                active_entities_by_type[entity_type_index];
        }
        for (int32 team_index{}; team_index < team_count; ++team_index) {
            auto const field{
                FieldMask::active_entities_by_team_and_type_field(team_index, entity_type_index)};
            if (mask.has(field)) {
                columns.active_entities_by_team_and_type[row][team_index][entity_type_index] =
                    current_state_.active_entities_by_team_and_type[team_index][entity_type_index];
            }
        }
    }
    if (mask.has(Field::Kills)) {
        columns.kills[row] = current_state_.kills;
        cumulative_kill_count_data_.add(tick, current_state_.kills);
    }
    if (mask.has(Field::RequestedTimeScale)) {
        columns.requested_time_scale[row] = time_scale;
    }

    payload_write_count_ += std::popcount(mask.value());
    last_sampled_state_.active_entities = current_state_.active_entities;
    last_sampled_state_.active_entities_by_team_and_type =
        current_state_.active_entities_by_team_and_type;
    last_sampled_state_.kills = current_state_.kills;
    last_sampled_time_scale_ = time_scale;
}

void FLevelTelemetryManager::sample_series() {
    using namespace level_telemetry_detail;
    sample_live_series();

    FieldMask mask;
    auto const mark_changed{
        [this, &mask](Field const field, auto const value, auto const previous) {
            if (!has_sampled_state_ || value != previous) {
                mask.set(field);
            }
        }};
    mark_changed(Field::SpawnedEntities,
                 current_state_.spawned_entities,
                 last_sampled_state_.spawned_entities);
    mark_changed(Field::DestroyedEntities,
                 current_state_.destroyed_entities,
                 last_sampled_state_.destroyed_entities);
    mark_changed(Field::RegistrySlotCount,
                 current_state_.registry_slot_count,
                 last_sampled_state_.registry_slot_count);
    mark_changed(
        Field::ActiveLasers, current_state_.active_lasers, last_sampled_state_.active_lasers);
    mark_changed(Field::LasersFired, current_state_.lasers_fired, last_sampled_state_.lasers_fired);
    mark_changed(Field::OccupiedSpatialCellCount,
                 current_state_.occupied_spatial_cell_count,
                 last_sampled_state_.occupied_spatial_cell_count);
    mark_changed(Field::GridRebuildCount,
                 current_state_.grid_rebuild_count,
                 last_sampled_state_.grid_rebuild_count);
    mark_changed(Field::RangeQueryCount,
                 current_state_.range_query_count,
                 last_sampled_state_.range_query_count);
    mark_changed(Field::LineTraceCount,
                 current_state_.line_trace_count,
                 last_sampled_state_.line_trace_count);
    mark_changed(Field::SweepTraceCount,
                 current_state_.sweep_trace_count,
                 last_sampled_state_.sweep_trace_count);

    if (!mask.is_empty()) {
        auto columns{append_history_row(clock_.get_completed_ticks())};
        constexpr int32 row{};
        columns.validity_masks[row].set(mask);

        if (mask.has(Field::SpawnedEntities)) {
            columns.spawned_entities[row] = current_state_.spawned_entities;
        }
        if (mask.has(Field::DestroyedEntities)) {
            columns.destroyed_entities[row] = current_state_.destroyed_entities;
        }
        if (mask.has(Field::RegistrySlotCount)) {
            columns.registry_slot_count[row] = current_state_.registry_slot_count;
        }
        if (mask.has(Field::ActiveLasers)) {
            columns.active_lasers[row] = current_state_.active_lasers;
        }
        if (mask.has(Field::LasersFired)) {
            columns.lasers_fired[row] = current_state_.lasers_fired;
        }
        if (mask.has(Field::OccupiedSpatialCellCount)) {
            columns.occupied_spatial_cell_count[row] = current_state_.occupied_spatial_cell_count;
        }
        if (mask.has(Field::GridRebuildCount)) {
            columns.grid_rebuild_count[row] = current_state_.grid_rebuild_count;
        }
        if (mask.has(Field::RangeQueryCount)) {
            columns.range_query_count[row] = current_state_.range_query_count;
        }
        if (mask.has(Field::LineTraceCount)) {
            columns.line_trace_count[row] = current_state_.line_trace_count;
        }
        if (mask.has(Field::SweepTraceCount)) {
            columns.sweep_trace_count[row] = current_state_.sweep_trace_count;
        }
        payload_write_count_ += std::popcount(mask.value());
    }

    last_sampled_state_.spawned_entities = current_state_.spawned_entities;
    last_sampled_state_.destroyed_entities = current_state_.destroyed_entities;
    last_sampled_state_.registry_slot_count = current_state_.registry_slot_count;
    last_sampled_state_.active_lasers = current_state_.active_lasers;
    last_sampled_state_.lasers_fired = current_state_.lasers_fired;
    last_sampled_state_.occupied_spatial_cell_count = current_state_.occupied_spatial_cell_count;
    last_sampled_state_.grid_rebuild_count = current_state_.grid_rebuild_count;
    last_sampled_state_.range_query_count = current_state_.range_query_count;
    last_sampled_state_.line_trace_count = current_state_.line_trace_count;
    last_sampled_state_.sweep_trace_count = current_state_.sweep_trace_count;
    has_sampled_state_ = true;
}

auto FLevelTelemetryManager::append_history_row(tick_type const completed_tick)
    -> ml::level_telemetry::FHistoryRowsView {
    auto const row_count{history_.num()};
    if (row_count > 0) {
        auto const completed_ticks{history_.last_const_view().completed_ticks()};
        if (completed_ticks[0] == completed_tick) {
            return history_.last_view().columns();
        }
        check(completed_ticks[0] < completed_tick);
    }

    auto columns{history_.append_uninitialized().columns()};
    columns.completed_ticks[0] = completed_tick;
    columns.validity_masks[0] = {};
    return columns;
}

auto FLevelTelemetryManager::get_history_stats() const noexcept -> FLevelTelemetryHistoryStats {
    auto const block_stats{history_.get_stats()};
    return {
        .configured_block_bytes = block_stats.configured_block_bytes,
        .layout_bytes_per_block = block_stats.layout_bytes_per_block,
        .rows_per_block = block_stats.rows_per_block,
        .acquired_block_count = block_stats.acquired_block_count,
        .retained_block_count = block_stats.retained_block_count,
        .peak_block_count = block_stats.peak_block_count,
        .total_sample_capacity = block_stats.total_sample_capacity,
        .total_byte_capacity = block_stats.total_byte_capacity,
        .used_sample_count = block_stats.used_sample_count,
        .used_payload_bytes = block_stats.used_payload_bytes,
        .unused_samples_in_final_block = block_stats.unused_samples_in_final_block,
        .unused_payload_bytes_in_final_block = block_stats.unused_payload_bytes_in_final_block,
        .fixed_layout_overhead_bytes = block_stats.fixed_layout_overhead_bytes,
        .payload_write_count = payload_write_count_,
    };
}

auto FLevelTelemetryManager::materialize_tick_series() const -> FLevelTelemetryTickSeries {
    using namespace level_telemetry_detail;
    TStaticArray<int32, FieldMask::field_count> sample_counts{};
    history_.for_each_block([&sample_counts](auto const block) {
        auto const rows{block.columns()};
        for (auto const mask : rows.validity_masks) {
            auto remaining{mask.value()};
            while (remaining != 0) {
                auto const field{static_cast<int32>(std::countr_zero(remaining))};
                ++sample_counts[field];
                remaining &= remaining - 1;
            }
        }
    });

    FLevelTelemetryTickSeries result;
    result.active_entities.reserve(sample_counts[FieldMask::index(Field::ActiveEntities)]);
    constexpr auto team_count{FLevelTelemetryTickSeries::team_count};
    constexpr auto entity_type_count{FLevelTelemetryTickSeries::entity_type_count};
    for (int32 entity_type_index{}; entity_type_index < entity_type_count; ++entity_type_index) {
        result.active_entities_by_type[entity_type_index].reserve(sample_counts[FieldMask::index(
            FieldMask::active_entities_by_type_field(entity_type_index))]);
        for (int32 team_index{}; team_index < team_count; ++team_index) {
            auto const field{
                FieldMask::active_entities_by_team_and_type_field(team_index, entity_type_index)};
            result.active_entities_by_team_and_type[team_index][entity_type_index].reserve(
                sample_counts[FieldMask::index(field)]);
        }
    }
    result.spawned_entities.reserve(sample_counts[FieldMask::index(Field::SpawnedEntities)]);
    result.destroyed_entities.reserve(sample_counts[FieldMask::index(Field::DestroyedEntities)]);
    result.kills.reserve(sample_counts[FieldMask::index(Field::Kills)]);
    result.registry_slot_count.reserve(sample_counts[FieldMask::index(Field::RegistrySlotCount)]);
    result.active_lasers.reserve(sample_counts[FieldMask::index(Field::ActiveLasers)]);
    result.lasers_fired.reserve(sample_counts[FieldMask::index(Field::LasersFired)]);
    result.occupied_spatial_cell_count.reserve(
        sample_counts[FieldMask::index(Field::OccupiedSpatialCellCount)]);
    result.grid_rebuild_count.reserve(sample_counts[FieldMask::index(Field::GridRebuildCount)]);
    result.range_query_count.reserve(sample_counts[FieldMask::index(Field::RangeQueryCount)]);
    result.line_trace_count.reserve(sample_counts[FieldMask::index(Field::LineTraceCount)]);
    result.sweep_trace_count.reserve(sample_counts[FieldMask::index(Field::SweepTraceCount)]);
    result.requested_time_scale.reserve(sample_counts[FieldMask::index(Field::RequestedTimeScale)]);

    history_.for_each_block([&result](auto const block) {
        auto const rows{block.columns()};
        auto const row_count{rows.num()};
        for (int32 row{}; row < row_count; ++row) {
            auto const tick{rows.completed_ticks[row]};
            auto const mask{rows.validity_masks[row]};
            if (mask.has(Field::ActiveEntities)) {
                result.active_entities.add(tick, rows.active_entities[row]);
            }
            for (int32 entity_type_index{}; entity_type_index < entity_type_count;
                 ++entity_type_index) {
                auto const type_field{FieldMask::active_entities_by_type_field(entity_type_index)};
                if (mask.has(type_field)) {
                    result.active_entities_by_type[entity_type_index].add(
                        tick, rows.active_entities_by_type[row][entity_type_index]);
                }
                for (int32 team_index{}; team_index < team_count; ++team_index) {
                    auto const field{FieldMask::active_entities_by_team_and_type_field(
                        team_index, entity_type_index)};
                    if (mask.has(field)) {
                        result.active_entities_by_team_and_type[team_index][entity_type_index].add(
                            tick,
                            rows.active_entities_by_team_and_type[row][team_index]
                                                                 [entity_type_index]);
                    }
                }
            }
            if (mask.has(Field::SpawnedEntities)) {
                result.spawned_entities.add(tick, rows.spawned_entities[row]);
            }
            if (mask.has(Field::DestroyedEntities)) {
                result.destroyed_entities.add(tick, rows.destroyed_entities[row]);
            }
            if (mask.has(Field::Kills)) {
                result.kills.add(tick, rows.kills[row]);
            }
            if (mask.has(Field::RegistrySlotCount)) {
                result.registry_slot_count.add(tick, rows.registry_slot_count[row]);
            }
            if (mask.has(Field::ActiveLasers)) {
                result.active_lasers.add(tick, rows.active_lasers[row]);
            }
            if (mask.has(Field::LasersFired)) {
                result.lasers_fired.add(tick, rows.lasers_fired[row]);
            }
            if (mask.has(Field::OccupiedSpatialCellCount)) {
                result.occupied_spatial_cell_count.add(tick, rows.occupied_spatial_cell_count[row]);
            }
            if (mask.has(Field::GridRebuildCount)) {
                result.grid_rebuild_count.add(tick, rows.grid_rebuild_count[row]);
            }
            if (mask.has(Field::RangeQueryCount)) {
                result.range_query_count.add(tick, rows.range_query_count[row]);
            }
            if (mask.has(Field::LineTraceCount)) {
                result.line_trace_count.add(tick, rows.line_trace_count[row]);
            }
            if (mask.has(Field::SweepTraceCount)) {
                result.sweep_trace_count.add(tick, rows.sweep_trace_count[row]);
            }
            if (mask.has(Field::RequestedTimeScale)) {
                result.requested_time_scale.add(tick, rows.requested_time_scale[row]);
            }
        }
    });
    return result;
}

void FLevelTelemetryManager::sample_battle_state(bool const force) {
    auto const tick{clock_.get_completed_ticks()};
    auto& samples{battle_samples_};
    if (!force && !samples.IsEmpty() && samples.Last().completed_tick == tick) {
        return;
    }

    if (force && !samples.IsEmpty() && samples.Last().completed_tick == tick) {
        samples.Pop(EAllowShrinking::No);
    }

    samples.Add(FLevelTelemetryBattleSample{
        .completed_tick = tick,
        .simulated_elapsed_seconds = clock_.get_simulation_time(),
        .combat = entity_registry_.get_combat_telemetry(),
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

/* **************************************** */
// Run capture
/* **************************************** */
void FLevelTelemetryManager::begin_run(FLevelTelemetryRunMetadata metadata) {
    check(initialized_);
    check(clock_.get_tick_rate() > 0.0);
    check(clock_.get_tick_period() > 0.0);
    check(clock_.get_time_scale() > 0.0);
    check(!active_entity_count_data_.is_empty());
    check(has_sampled_state_);

    metadata.tick_rate_hz = clock_.get_tick_rate();
    metadata.tick_period_seconds = clock_.get_tick_period();
    metadata.initial_requested_time_scale = clock_.get_time_scale();

    metadata_ = MoveTemp(metadata);
    completion_ = {};
    completed_ticks_by_real_time_.reset();
    run_started_at_ = FPlatformTime::Seconds();
    run_recording_ = true;
    run_finalized_ = false;
    run_record_taken_ = false;

    add_realtime_sample(clock_.get_completed_ticks(), run_started_at_);
    sample_battle_state(true);
    next_battle_sample_seconds_ = 1.0;
}

void FLevelTelemetryManager::capture_realtime_sample() {
    if (!run_recording_) {
        return;
    }

    auto const now{FPlatformTime::Seconds()};
    add_realtime_sample(clock_.get_completed_ticks(), now);
    close_performance_window(now);
}

void FLevelTelemetryManager::close_performance_window(double const monotonic_time) {
    FSimulationTelemetryPerformanceWindow window;
    window.real_elapsed_seconds = wall_elapsed(monotonic_time);
    window.completed_tick = clock_.get_completed_ticks();
    window.frame = aggregate_timings(MoveTemp(frame_samples_));
    window.game_thread = window.frame;
    window.simulation_tick = aggregate_timings(MoveTemp(simulation_tick_samples_));

    for (int32 index{}; index < FSimulationTelemetryPerformanceWindow::system_count; ++index) {
        window.systems[index] = aggregate_timings(MoveTemp(system_samples_[index]));
        system_samples_[index].Reset();
    }

    double phase_total_ms{};
    for (int32 index{}; index < FSimulationTelemetryPerformanceWindow::phase_count; ++index) {
        window.phases[index] = aggregate_timings(MoveTemp(phase_samples_[index]));
        phase_total_ms += window.phases[index].mean_ms;
        phase_samples_[index].Reset();
    }

    if (phase_total_ms > 0.0) {
        for (int32 index{}; index < FSimulationTelemetryPerformanceWindow::phase_count; ++index) {
            window.phase_cpu_share[index] = window.phases[index].mean_ms / phase_total_ms;
        }
    }

    performance_windows_.Add(MoveTemp(window));
    frame_samples_.Reset();
    simulation_tick_samples_.Reset();
}

/* **************************************** */
// Completion and finalization
/* **************************************** */
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
    completion_.winning_team = winning_team;
}

auto FLevelTelemetryManager::take_finalized_run() -> TOptional<FLevelTelemetryRunRecord> {
    if (!run_finalized_ || run_record_taken_) {
        return NullOpt;
    }

    run_record_taken_ = true;

    FLevelTelemetryRunRecord result;
    result.metadata = MoveTemp(metadata_);
    result.completion = MoveTemp(completion_);
    result.tick_series = materialize_tick_series();
    result.completed_ticks_by_real_time = MoveTemp(completed_ticks_by_real_time_);
    result.battle_samples = MoveTemp(battle_samples_);
    result.performance_windows = MoveTemp(performance_windows_);
    return result;
}

auto FLevelTelemetryManager::wall_elapsed(double const monotonic_time) const -> double {
    return FMath::Max(0.0, monotonic_time - run_started_at_);
}

void FLevelTelemetryManager::add_realtime_sample(tick_type const completed_tick,
                                                 double const monotonic_time) {
    auto& data{completed_ticks_by_real_time_};
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

    auto const completed_ticks{clock_.get_completed_ticks()};

    update_current_state();
    sample_series();
    sample_battle_state(true);
    add_realtime_sample(completed_ticks, monotonic_time);
    if (!frame_samples_.IsEmpty() || !simulation_tick_samples_.IsEmpty()) {
        close_performance_window(monotonic_time);
    }

    completion_.reason = reason;
    completion_.interrupted = interrupted;
    completion_.completed_utc = FDateTime::UtcNow().ToIso8601();
    completion_.world_end_reason = MoveTemp(world_end_reason);
    completion_.completed_ticks = completed_ticks;
    completion_.simulated_elapsed_seconds = clock_.get_simulation_time();
    completion_.wall_elapsed_seconds = wall_elapsed(monotonic_time);

    if (mission_result != nullptr) {
        completion_.mission_mode = mission_result->mode;
        completion_.mission_state = mission_result->state;
        completion_.mission_fail_reason = mission_result->fail_reason;
        completion_.mission_elapsed_seconds = mission_result->elapsed_seconds;
    }

    run_recording_ = false;
    run_finalized_ = true;
}
