#include "ioj/sim/level_telemetry_manager.h"
#include <cstdint>
#include <optional>
#include <sandbox/core/monotonic_clock.h>
#include <span>
#include <vector>

#include <ioj/sim/lasers/sim.h>
#include <ioj/sim/mission_manager.h>
#include <ioj/sim/sim_clock.h>

#include <ioj/sim/telemetry_statistics.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <format>

#include <bit>

namespace ioj::sim {

namespace level_telemetry_detail {
auto utc_now_iso8601() -> std::string {
    auto const now{std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now())};
    return std::format("{:%FT%TZ}", now);
}

using Field = ioj::sim::telemetry::HistoryField;
using FieldMask = ioj::sim::telemetry::HistoryFieldMask;

template <typename Data>
auto snapshot_with_terminal_sample(Data const& source,
                                   LevelTelemetryManager::tick_type const completed_tick) -> Data {
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
    for (std::int32_t i{0}; i < sample_count; ++i) {
        if (data.value_at(i) < 0) {
            return false;
        }
    }
    return true;
}

auto aggregate_timings(std::vector<double> samples) -> LevelTelemetryTimingAggregate {
    return ioj::sim::telemetry::aggregate_timings(
        {samples.data(), static_cast<std::size_t>(samples.size())});
}

}

using level_telemetry_detail::aggregate_timings;
using level_telemetry_detail::contains_only_nonnegative_values;
using level_telemetry_detail::snapshot_with_terminal_sample;

/* **************************************** */
// Construction and lifecycle
/* **************************************** */
LevelTelemetryManager::LevelTelemetryManager(SimClock const& clock,
                                             EntityRegistry const& entity_registry,
                                             ioj::sim::lasers::Sim const& lasers,
                                             ioj::sim::SpatialQueryManager const& spatial_queries,
                                             GameMemory& game_memory,
                                             LevelTelemetryHistoryConfig history_config) noexcept
    : clock_{clock}
    , entity_registry_{entity_registry}
    , lasers_{lasers}
    , spatial_queries_{spatial_queries}
    , history_{game_memory, history_config} {}

void LevelTelemetryManager::initialise() {
    reset();
    initialized_ = true;
    update_current_state();
    sample_series();
}

void LevelTelemetryManager::reset() {
    current_state_ = {};
    last_sampled_state_ = {};
    history_.reset();
    metadata_ = {};
    completion_ = {};
    completed_ticks_by_real_time_.reset();
    battle_samples_.clear();
    performance_windows_.clear();
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

    frame_samples_.clear();
    simulation_tick_samples_.clear();
    for (auto& samples : system_samples_) {
        samples.clear();
    }
    for (auto& samples : phase_samples_) {
        samples.clear();
    }
}

/* **************************************** */
// Tick sampling
/* **************************************** */
void LevelTelemetryManager::tick() {
    assert(initialized_);
    update_current_state();
    sample_live_series();

    auto const simulated_seconds{clock_.get_simulation_time()};
    if (simulated_seconds + 1.e-8 >= next_battle_sample_seconds_) {
        sample_series();
        sample_battle_state();
        next_battle_sample_seconds_ = std::floor(simulated_seconds) + 1.0;
    }
}

void LevelTelemetryManager::observe_frame(double const frame_seconds) {
    if (run_recording_ && frame_seconds >= 0.0) {
        frame_samples_.push_back(frame_seconds);
    }
}

void LevelTelemetryManager::record_simulation_tick_timing(
    double const elapsed_seconds,
    std::array<double, SimTelemetryPerformanceWindow::system_count> const& systems,
    std::array<double, SimTelemetryPerformanceWindow::phase_count> const& phases) {
    if (!run_recording_ || !metadata_.detailed_timing) {
        return;
    }

    simulation_tick_samples_.push_back(elapsed_seconds);
    for (std::int32_t index{}; index < SimTelemetryPerformanceWindow::system_count; ++index) {
        if (systems[index] >= 0.0) {
            system_samples_[index].push_back(systems[index]);
        }
    }

    for (std::int32_t index{}; index < SimTelemetryPerformanceWindow::phase_count; ++index) {
        if (phases[index] >= 0.0) {
            phase_samples_[index].push_back(phases[index]);
        }
    }
}

/* **************************************** */
// Snapshots
/* **************************************** */
auto LevelTelemetryManager::make_snapshot() const -> LevelTelemetrySnapshot {
    assert(initialized_);
    auto const completed_tick{clock_.get_completed_ticks()};
    auto const tick_period{clock_.get_tick_period()};

    assert((std::isfinite(tick_period) && tick_period > 0.0) &&
           "Level telemetry snapshots require a finite, positive tick period.");
    auto const& active_entity_count_data{active_entity_count_data_};
    auto const& cumulative_kill_count_data{cumulative_kill_count_data_};
    assert((!active_entity_count_data.is_empty() && !cumulative_kill_count_data.is_empty()) &&
           "Level telemetry must be initialised before creating a snapshot.");
    assert((active_entity_count_data.last_time() <= completed_tick &&
            cumulative_kill_count_data.last_time() <= completed_tick) &&
           "Level telemetry cannot contain samples after the snapshot tick.");
    assert((contains_only_nonnegative_values(active_entity_count_data) &&
            contains_only_nonnegative_values(cumulative_kill_count_data) &&
            current_state_.spawned_entities >= 0 && current_state_.active_lasers >= 0 &&
            current_state_.lasers_fired >= 0) &&
           "Level telemetry snapshots cannot contain negative counts.");

    LevelTelemetrySnapshot snapshot;
    snapshot.elapsed_seconds = static_cast<double>(completed_tick) * tick_period;
    snapshot.tick_period = tick_period;
    snapshot.active_entities = current_state_.active_entities;
    snapshot.spawned_entities = current_state_.spawned_entities;

    assert((snapshot.spawned_entities >= snapshot.active_entities) &&
           "Spawned entity count cannot be lower than active entity count.");
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
void LevelTelemetryManager::update_current_state() {
    auto const spatial{spatial_queries_.get_runtime_telemetry()};
    current_state_.active_entities_by_team_and_type =
        entity_registry_.count_alive_per_team_and_type();
    current_state_.active_entities = entity_registry_.count_alive();
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

    assert(current_state_.destroyed_entities >= 0);
}

void LevelTelemetryManager::sample_live_series() {
    using namespace level_telemetry_detail;
    constexpr auto team_count{LevelTelemetryTickSeries::team_count};
    constexpr auto entity_type_count{LevelTelemetryTickSeries::entity_type_count};
    EntityRegistry::EntityTypeCounts active_entities_by_type{};
    EntityRegistry::EntityTypeCounts last_active_entities_by_type{};
    FieldMask mask;

    if (!has_sampled_state_ ||
        current_state_.active_entities != last_sampled_state_.active_entities) {
        mask.set(Field::ActiveEntities);
    }

    for (std::int32_t entity_type_index{}; entity_type_index < entity_type_count;
         ++entity_type_index) {
        for (std::int32_t team_index{}; team_index < team_count; ++team_index) {
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
    constexpr std::int32_t row{};
    columns.validity_masks[row].set(mask);

    if (mask.has(Field::ActiveEntities)) {
        columns.active_entities[row] = current_state_.active_entities;
        active_entity_count_data_.add(tick, current_state_.active_entities);
    }
    for (std::int32_t entity_type_index{}; entity_type_index < entity_type_count;
         ++entity_type_index) {
        if (mask.has(FieldMask::active_entities_by_type_field(entity_type_index))) {
            columns.active_entities_by_type[row][entity_type_index] =
                active_entities_by_type[entity_type_index];
        }
        for (std::int32_t team_index{}; team_index < team_count; ++team_index) {
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

void LevelTelemetryManager::sample_series() {
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
        constexpr std::int32_t row{};
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

auto LevelTelemetryManager::append_history_row(tick_type const completed_tick)
    -> ioj::sim::telemetry::HistoryRowsView {
    auto const row_count{history_.num()};
    if (row_count > 0) {
        auto const completed_ticks{history_.last_const_view().completed_ticks()};
        if (completed_ticks[0] == completed_tick) {
            return history_.last_view().columns();
        }
        assert(completed_ticks[0] < completed_tick);
    }

    auto columns{history_.append_uninitialized().columns()};
    columns.completed_ticks[0] = completed_tick;
    columns.validity_masks[0] = {};
    return columns;
}

auto LevelTelemetryManager::get_history_stats() const noexcept -> LevelTelemetryHistoryStats {
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

auto LevelTelemetryManager::materialize_tick_series() const -> LevelTelemetryTickSeries {
    using namespace level_telemetry_detail;
    std::array<std::int32_t, FieldMask::field_count> sample_counts{};
    history_.for_each_block([&sample_counts](auto const block) {
        auto const rows{block.columns()};
        for (auto const mask : rows.validity_masks) {
            auto remaining{mask.value()};
            while (remaining != 0) {
                auto const field{static_cast<std::int32_t>(std::countr_zero(remaining))};
                ++sample_counts[field];
                remaining &= remaining - 1;
            }
        }
    });

    LevelTelemetryTickSeries result;
    result.active_entities.reserve(sample_counts[FieldMask::index(Field::ActiveEntities)]);
    constexpr auto team_count{LevelTelemetryTickSeries::team_count};
    constexpr auto entity_type_count{LevelTelemetryTickSeries::entity_type_count};
    for (std::int32_t entity_type_index{}; entity_type_index < entity_type_count;
         ++entity_type_index) {
        result.active_entities_by_type[entity_type_index].reserve(sample_counts[FieldMask::index(
            FieldMask::active_entities_by_type_field(entity_type_index))]);
        for (std::int32_t team_index{}; team_index < team_count; ++team_index) {
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
        for (std::int32_t row{}; row < row_count; ++row) {
            auto const tick{rows.completed_ticks[row]};
            auto const mask{rows.validity_masks[row]};
            if (mask.has(Field::ActiveEntities)) {
                result.active_entities.add(tick, rows.active_entities[row]);
            }
            for (std::int32_t entity_type_index{}; entity_type_index < entity_type_count;
                 ++entity_type_index) {
                auto const type_field{FieldMask::active_entities_by_type_field(entity_type_index)};
                if (mask.has(type_field)) {
                    result.active_entities_by_type[entity_type_index].add(
                        tick, rows.active_entities_by_type[row][entity_type_index]);
                }
                for (std::int32_t team_index{}; team_index < team_count; ++team_index) {
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

void LevelTelemetryManager::sample_battle_state(bool const force) {
    auto const tick{clock_.get_completed_ticks()};
    auto& samples{battle_samples_};
    if (!force && !samples.empty() && samples.back().completed_tick == tick) {
        return;
    }

    if (force && !samples.empty() && samples.back().completed_tick == tick) {
        samples.pop_back();
    }

    samples.push_back(LevelTelemetryBattleSample{
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
void LevelTelemetryManager::begin_run(LevelTelemetryRunMetadata metadata) {
    assert(initialized_);
    assert(clock_.get_tick_rate() > 0.0);
    assert(clock_.get_tick_period() > 0.0);
    assert(clock_.get_time_scale() > 0.0);
    assert(!active_entity_count_data_.is_empty());
    assert(has_sampled_state_);

    metadata.tick_rate_hz = clock_.get_tick_rate();
    metadata.tick_period_seconds = clock_.get_tick_period();
    metadata.initial_requested_time_scale = clock_.get_time_scale();

    metadata_ = std::move(metadata);
    completion_ = {};
    completed_ticks_by_real_time_.reset();
    run_started_at_ = ml::monotonic_seconds();
    run_recording_ = true;
    run_finalized_ = false;
    run_record_taken_ = false;

    add_realtime_sample(clock_.get_completed_ticks(), run_started_at_);
    sample_battle_state(true);
    next_battle_sample_seconds_ = 1.0;
}

void LevelTelemetryManager::capture_realtime_sample() {
    if (!run_recording_) {
        return;
    }

    auto const now{ml::monotonic_seconds()};
    add_realtime_sample(clock_.get_completed_ticks(), now);
    close_performance_window(now);
}

void LevelTelemetryManager::close_performance_window(double const monotonic_time) {
    SimTelemetryPerformanceWindow window;
    window.real_elapsed_seconds = wall_elapsed(monotonic_time);
    window.completed_tick = clock_.get_completed_ticks();
    window.frame = aggregate_timings(std::move(frame_samples_));
    window.game_thread = window.frame;
    window.simulation_tick = aggregate_timings(std::move(simulation_tick_samples_));

    for (std::int32_t index{}; index < SimTelemetryPerformanceWindow::system_count; ++index) {
        window.systems[index] = aggregate_timings(std::move(system_samples_[index]));
        system_samples_[index].clear();
    }

    double phase_total_ms{};
    for (std::int32_t index{}; index < SimTelemetryPerformanceWindow::phase_count; ++index) {
        window.phases[index] = aggregate_timings(std::move(phase_samples_[index]));
        phase_total_ms += window.phases[index].mean_ms;
        phase_samples_[index].clear();
    }

    if (phase_total_ms > 0.0) {
        for (std::int32_t index{}; index < SimTelemetryPerformanceWindow::phase_count; ++index) {
            window.phase_cpu_share[index] = window.phases[index].mean_ms / phase_total_ms;
        }
    }

    performance_windows_.push_back(std::move(window));
    frame_samples_.clear();
    simulation_tick_samples_.clear();
}

/* **************************************** */
// Completion and finalization
/* **************************************** */
void LevelTelemetryManager::mark_mission_terminal(LevelMissionResult const& result) {
    auto const reason{result.state == ioj::sim::MissionState::Succeeded
                          ? ioj::sim::LevelTelemetryRunEndReason::MissionSucceeded
                          : ioj::sim::LevelTelemetryRunEndReason::MissionFailed};
    finalize_run(reason, false, {}, &result, ml::monotonic_seconds());
}

void LevelTelemetryManager::finalize_interrupted(ioj::sim::LevelTelemetryRunEndReason const reason,
                                                 std::string world_end_reason) {
    finalize_run(reason, true, std::move(world_end_reason), nullptr, ml::monotonic_seconds());
}

void LevelTelemetryManager::finalize_completed(ioj::sim::LevelTelemetryRunEndReason const reason,
                                               std::optional<ioj::sim::Team> winning_team) {
    finalize_run(reason, false, {}, nullptr, ml::monotonic_seconds());
    completion_.winning_team = winning_team;
}

auto LevelTelemetryManager::take_finalized_run() -> std::optional<LevelTelemetryRunRecord> {
    if (!run_finalized_ || run_record_taken_) {
        return std::nullopt;
    }

    run_record_taken_ = true;

    LevelTelemetryRunRecord result;
    result.metadata = std::move(metadata_);
    result.completion = std::move(completion_);
    result.tick_series = materialize_tick_series();
    result.completed_ticks_by_real_time = std::move(completed_ticks_by_real_time_);
    result.battle_samples = std::move(battle_samples_);
    result.performance_windows = std::move(performance_windows_);
    return result;
}

auto LevelTelemetryManager::wall_elapsed(double const monotonic_time) const -> double {
    return std::max(0.0, monotonic_time - run_started_at_);
}

void LevelTelemetryManager::add_realtime_sample(tick_type const completed_tick,
                                                double const monotonic_time) {
    auto& data{completed_ticks_by_real_time_};
    auto const elapsed{wall_elapsed(monotonic_time)};
    if (data.is_empty() || data.last_time() < elapsed) {
        data.add(elapsed, completed_tick);
    }
}

void LevelTelemetryManager::finalize_run(ioj::sim::LevelTelemetryRunEndReason const reason,
                                         bool const interrupted,
                                         std::string world_end_reason,
                                         LevelMissionResult const* const mission_result,
                                         double const monotonic_time) {
    if (!run_recording_ || run_finalized_) {
        return;
    }

    auto const completed_ticks{clock_.get_completed_ticks()};

    update_current_state();
    sample_series();
    sample_battle_state(true);
    add_realtime_sample(completed_ticks, monotonic_time);
    if (!frame_samples_.empty() || !simulation_tick_samples_.empty()) {
        close_performance_window(monotonic_time);
    }

    completion_.reason = reason;
    completion_.interrupted = interrupted;
    completion_.completed_utc = level_telemetry_detail::utc_now_iso8601();
    completion_.world_end_reason = std::move(world_end_reason);
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
} // namespace ioj::sim
