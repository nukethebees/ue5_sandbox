#include "ioj/sim/telemetry/manager.h"

#include <ioj/sim/lasers/sim.h>
#include <ioj/sim/sim_clock.h>

#include <algorithm>
#include <bit>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <vector>

namespace ioj::sim {

namespace level_telemetry_detail {
auto utc_now_iso8601() -> std::string {
    auto const now{std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now())};
    return std::format("{:%FT%TZ}", now);
}

using Field = telemetry::HistoryField;
using FieldMask = telemetry::HistoryFieldMask;

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
    for (std::uint32_t i{0}; i < sample_count; ++i) {
        if (data.value_at(i) < 0) {
            return false;
        }
    }
    return true;
}

}

using level_telemetry_detail::contains_only_nonnegative_values;
using level_telemetry_detail::snapshot_with_terminal_sample;

/* **************************************** */
// Construction and lifecycle
/* **************************************** */
LevelTelemetryManager::LevelTelemetryManager(SimClock const& clock,
                                             EntityLedger const& entity_ledger,
                                             lasers::Sim const& lasers,
                                             GameMemory& game_memory,
                                             LevelTelemetryHistoryConfig history_config) noexcept
    : clock_{clock}
    , entity_ledger_{entity_ledger}
    , lasers_{lasers}
    , history_{game_memory, history_config} {}

void LevelTelemetryManager::initialise() {
    assert(state_ == State::Uninitialised);

    if (metadata_.launched_utc.empty()) {
        metadata_.launched_utc = level_telemetry_detail::utc_now_iso8601();
    }

    update_current_state();
    sample_series();

    sample_battle_state();
    state_ = State::Recording;
}

void LevelTelemetryManager::reset() {
    current_state_ = {};
    last_sampled_state_ = {};
    history_.reset();
    metadata_ = {};
    completion_ = {};
    battle_samples_.clear();
    active_entity_count_data_.reset();
    cumulative_kill_count_data_.reset();
    state_ = State::Uninitialised;
    next_battle_sample_seconds_ = 0.0;
}

/* **************************************** */
// Tick sampling
/* **************************************** */
void LevelTelemetryManager::tick() {
    assert(state_ != State::Uninitialised);
    update_current_state();
    sample_live_series();

    auto const simulated_seconds{clock_.get_simulation_time()};
    if (simulated_seconds + 1.e-8 >= next_battle_sample_seconds_) {
        sample_series();
        sample_battle_state();
        next_battle_sample_seconds_ = std::floor(simulated_seconds) + 1.0;
    }
}

/* **************************************** */
// Snapshots
/* **************************************** */
auto LevelTelemetryManager::make_snapshot() const -> LevelTelemetrySnapshot {
    assert(state_ != State::Uninitialised);
    auto const completed_tick{clock_.get_completed_ticks()};
    auto const tick_rate_hz{clock_.get_tick_rate()};
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
    snapshot.elapsed_seconds =
        Duration::make(static_cast<double>(completed_tick) * tick_rate_hz.period().get_seconds());
    snapshot.tick_rate_hz = tick_rate_hz;
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
    current_state_.active_entities_by_team_and_type =
        entity_ledger_.count_alive_per_team_and_type();
    current_state_.active_entities = entity_ledger_.count_alive();
    current_state_.spawned_entities = entity_ledger_.get_num_unique_ids_issued();
    current_state_.destroyed_entities =
        current_state_.spawned_entities - current_state_.active_entities;
    current_state_.kills = entity_ledger_.count_kills();
    current_state_.active_lasers = lasers_.get_num_instances();
    current_state_.lasers_fired = lasers_.get_number_spawned();

    assert(current_state_.destroyed_entities >= 0);
}

void LevelTelemetryManager::sample_live_series() {
    using namespace level_telemetry_detail;
    constexpr auto team_count{LevelTelemetryTickSeries::team_count};
    constexpr auto entity_type_count{LevelTelemetryTickSeries::entity_type_count};
    telemetry::EntityTypeCounts active_entities_by_type{};
    telemetry::EntityTypeCounts last_active_entities_by_type{};
    auto const initial_sample{state_ == State::Uninitialised};
    FieldMask mask;

    if (initial_sample || current_state_.active_entities != last_sampled_state_.active_entities) {
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
            if (initial_sample || value != last_value) {
                mask.set(FieldMask::active_entities_by_team_and_type_field(team_index,
                                                                           entity_type_index));
            }
        }
        if (initial_sample || active_entities_by_type[entity_type_index] !=
                                  last_active_entities_by_type[entity_type_index]) {
            mask.set(FieldMask::active_entities_by_type_field(entity_type_index));
        }
    }

    if (initial_sample || current_state_.kills != last_sampled_state_.kills) {
        mask.set(Field::Kills);
    }
    if (mask.is_empty()) {
        return;
    }

    auto const tick{clock_.get_completed_ticks()};
    auto columns{append_history_row(tick)};
    constexpr std::int32_t row{};
    columns.validity_masks()[row].set(mask);

    if (mask.has(Field::ActiveEntities)) {
        columns.active_entities()[row] = current_state_.active_entities;
        active_entity_count_data_.add(tick, current_state_.active_entities);
    }
    auto const counts_by_type{columns.active_entities_by_type()};
    auto const counts_by_team_and_type{columns.active_entities_by_team_and_type()};
    for (std::int32_t entity_type_index{}; entity_type_index < entity_type_count;
         ++entity_type_index) {
        if (mask.has(FieldMask::active_entities_by_type_field(entity_type_index))) {
            counts_by_type[row][entity_type_index] = active_entities_by_type[entity_type_index];
        }
        for (std::int32_t team_index{}; team_index < team_count; ++team_index) {
            auto const field{
                FieldMask::active_entities_by_team_and_type_field(team_index, entity_type_index)};
            if (mask.has(field)) {
                counts_by_team_and_type[row][team_index][entity_type_index] =
                    current_state_.active_entities_by_team_and_type[team_index][entity_type_index];
            }
        }
    }
    if (mask.has(Field::Kills)) {
        columns.kills()[row] = current_state_.kills;
        cumulative_kill_count_data_.add(tick, current_state_.kills);
    }

    last_sampled_state_.active_entities = current_state_.active_entities;
    last_sampled_state_.active_entities_by_team_and_type =
        current_state_.active_entities_by_team_and_type;
    last_sampled_state_.kills = current_state_.kills;
}

void LevelTelemetryManager::sample_series() {
    using namespace level_telemetry_detail;
    sample_live_series();

    auto const initial_sample{state_ == State::Uninitialised};
    FieldMask mask;
    auto const mark_changed{
        [initial_sample, &mask](Field const field, auto const value, auto const previous) {
            if (initial_sample || value != previous) {
                mask.set(field);
            }
        }};
    mark_changed(Field::SpawnedEntities,
                 current_state_.spawned_entities,
                 last_sampled_state_.spawned_entities);
    mark_changed(Field::DestroyedEntities,
                 current_state_.destroyed_entities,
                 last_sampled_state_.destroyed_entities);
    mark_changed(
        Field::ActiveLasers, current_state_.active_lasers, last_sampled_state_.active_lasers);
    mark_changed(Field::LasersFired, current_state_.lasers_fired, last_sampled_state_.lasers_fired);

    if (!mask.is_empty()) {
        auto columns{append_history_row(clock_.get_completed_ticks())};
        constexpr std::int32_t row{};
        columns.validity_masks()[row].set(mask);

        if (mask.has(Field::SpawnedEntities)) {
            columns.spawned_entities()[row] = current_state_.spawned_entities;
        }
        if (mask.has(Field::DestroyedEntities)) {
            columns.destroyed_entities()[row] = current_state_.destroyed_entities;
        }
        if (mask.has(Field::ActiveLasers)) {
            columns.active_lasers()[row] = current_state_.active_lasers;
        }
        if (mask.has(Field::LasersFired)) {
            columns.lasers_fired()[row] = current_state_.lasers_fired;
        }
    }

    last_sampled_state_.spawned_entities = current_state_.spawned_entities;
    last_sampled_state_.destroyed_entities = current_state_.destroyed_entities;
    last_sampled_state_.active_lasers = current_state_.active_lasers;
    last_sampled_state_.lasers_fired = current_state_.lasers_fired;
}

auto LevelTelemetryManager::append_history_row(tick_type const completed_tick)
    -> telemetry::HistoryRows::View {
    auto const row_count{history_.num()};
    if (row_count > 0) {
        auto const completed_ticks{history_.last_const_view().completed_ticks()};
        if (completed_ticks[0] == completed_tick) {
            return history_.last_view();
        }
        assert(completed_ticks[0] < completed_tick);
    }

    auto columns{history_.append_uninitialized()};
    columns.completed_ticks()[0] = completed_tick;
    columns.validity_masks()[0] = {};
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
    };
}

auto LevelTelemetryManager::materialize_tick_series() const -> LevelTelemetryTickSeries {
    using namespace level_telemetry_detail;
    std::array<std::uint32_t, FieldMask::field_count> sample_counts{};
    history_.for_each_block([&sample_counts](auto const block) {
        auto const rows{block};
        for (auto const mask : rows.validity_masks()) {
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
    result.active_lasers.reserve(sample_counts[FieldMask::index(Field::ActiveLasers)]);
    result.lasers_fired.reserve(sample_counts[FieldMask::index(Field::LasersFired)]);

    history_.for_each_block([&result](auto const block) {
        auto const rows{block};
        auto const row_count{rows.num()};
        auto const completed_ticks{rows.completed_ticks()};
        auto const validity_masks{rows.validity_masks()};
        auto const active_entities{rows.active_entities()};
        auto const active_entities_by_type{rows.active_entities_by_type()};
        auto const active_entities_by_team_and_type{rows.active_entities_by_team_and_type()};
        auto const spawned_entities{rows.spawned_entities()};
        auto const destroyed_entities{rows.destroyed_entities()};
        auto const kills{rows.kills()};
        auto const active_lasers{rows.active_lasers()};
        auto const lasers_fired{rows.lasers_fired()};

        for (std::uint32_t row{}; row < row_count; ++row) {
            auto const tick{completed_ticks[row]};
            auto const mask{validity_masks[row]};
            if (mask.has(Field::ActiveEntities)) {
                result.active_entities.add(tick, active_entities[row]);
            }
            for (std::int32_t entity_type_index{}; entity_type_index < entity_type_count;
                 ++entity_type_index) {
                auto const type_field{FieldMask::active_entities_by_type_field(entity_type_index)};
                if (mask.has(type_field)) {
                    result.active_entities_by_type[entity_type_index].add(
                        tick, active_entities_by_type[row][entity_type_index]);
                }
                for (std::int32_t team_index{}; team_index < team_count; ++team_index) {
                    auto const field{FieldMask::active_entities_by_team_and_type_field(
                        team_index, entity_type_index)};
                    if (mask.has(field)) {
                        result.active_entities_by_team_and_type[team_index][entity_type_index].add(
                            tick,
                            active_entities_by_team_and_type[row][team_index][entity_type_index]);
                    }
                }
            }
            if (mask.has(Field::SpawnedEntities)) {
                result.spawned_entities.add(tick, spawned_entities[row]);
            }
            if (mask.has(Field::DestroyedEntities)) {
                result.destroyed_entities.add(tick, destroyed_entities[row]);
            }
            if (mask.has(Field::Kills)) {
                result.kills.add(tick, kills[row]);
            }
            if (mask.has(Field::ActiveLasers)) {
                result.active_lasers.add(tick, active_lasers[row]);
            }
            if (mask.has(Field::LasersFired)) {
                result.lasers_fired.add(tick, lasers_fired[row]);
            }
        }
    });
    return result;
}

void LevelTelemetryManager::sample_battle_state() {
    auto const tick{clock_.get_completed_ticks()};
    auto& samples{battle_samples_};
    if (!samples.empty() && samples.back().completed_tick == tick) {
        samples.pop_back();
    }

    samples.push_back(LevelTelemetryBattleSample{
        .completed_tick = tick,
        .combat = entity_ledger_.get_combat_telemetry(),
        .alive = current_state_.active_entities_by_team_and_type,
        .active_lasers = current_state_.active_lasers,
        .lasers_fired = current_state_.lasers_fired,
    });
}

/* **************************************** */
// Run capture
/* **************************************** */
void LevelTelemetryManager::set_run_metadata(LevelTelemetryRunMetadata metadata) {
    assert(state_ == State::Uninitialised || state_ == State::Recording);

    if (metadata.launched_utc.empty()) {
        metadata.launched_utc = std::move(metadata_.launched_utc);
    }
    metadata_ = std::move(metadata);
}

/* **************************************** */
// Completion and finalization
/* **************************************** */

auto LevelTelemetryManager::take_finalized_run(Frequency const tick_rate_hz)
    -> std::optional<LevelTelemetryRunRecord> {
    if (state_ != State::Finalized) {
        return std::nullopt;
    }

    state_ = State::RecordTaken;

    return LevelTelemetryRunRecord{
        .metadata = std::move(metadata_),
        .tick_rate_hz = tick_rate_hz,
        .completion = std::move(completion_),
        .tick_series = materialize_tick_series(),
        .battle_samples = std::move(battle_samples_),
    };
}

void LevelTelemetryManager::finalize(LevelTelemetryRunEndReason const reason) {
    if (state_ != State::Recording) {
        return;
    }

    auto const completed_ticks{clock_.get_completed_ticks()};

    update_current_state();
    sample_series();
    sample_battle_state();

    completion_.reason = reason;
    completion_.completed_utc = level_telemetry_detail::utc_now_iso8601();
    completion_.completed_ticks = completed_ticks;

    state_ = State::Finalized;
}
} // namespace ioj::sim
