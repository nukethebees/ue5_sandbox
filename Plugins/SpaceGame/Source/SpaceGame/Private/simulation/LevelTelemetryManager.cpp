#include "SpaceGame/simulation/LevelTelemetryManager.h"

namespace {
auto are_equal(FTestEntityRegistry::EntityCounts const& lhs,
               FTestEntityRegistry::EntityCounts const& rhs) -> bool {
    constexpr auto team_count{ml::EnumCountTrait<ETestTeam>::count_value};
    constexpr auto type_count{ml::EnumCountTrait<ETestEntityType>::count_value};

    for (int32 team_index{}; team_index < team_count; ++team_index) {
        for (int32 type_index{}; type_index < type_count; ++type_index) {
            if (lhs[team_index][type_index] != rhs[team_index][type_index]) {
                return false;
            }
        }
    }

    return true;
}

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
        result.add(completed_tick, result.last_value());
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

void FLevelTelemetryManager::initialise(FTestEntityRegistry const& entity_registry,
                                        FLevelLaserTelemetry const lasers) {
    reset();
    sample_active_entity_counts(0, entity_registry);
    sample_cumulative_kill_count(0, entity_registry);
    sample_registry_slot_count(0, entity_registry);
    sample_issued_unique_id_count(0, entity_registry);
    sample_laser_counts(0, lasers);
}

void FLevelTelemetryManager::reset() {
    active_entity_count_data_.reset();
    active_entity_counts_data_.reset();
    cumulative_kill_count_data_.reset();
    registry_slot_count_data_.reset();
    issued_unique_id_count_data_.reset();
    active_laser_count_data_.reset();
    cumulative_laser_spawn_count_data_.reset();
}

void FLevelTelemetryManager::tick(tick_type const tick,
                                  FTestEntityRegistry const& entity_registry,
                                  FLevelLaserTelemetry const lasers) {
    sample_active_entity_counts(tick, entity_registry);
    sample_cumulative_kill_count(tick, entity_registry);
    sample_registry_slot_count(tick, entity_registry);
    sample_issued_unique_id_count(tick, entity_registry);
    sample_laser_counts(tick, lasers);
}

auto FLevelTelemetryManager::make_snapshot(tick_type const completed_tick,
                                           double const tick_period) const
    -> FLevelTelemetrySnapshot {
    checkf(FMath::IsFinite(tick_period) && tick_period > 0.0,
           TEXT("Level telemetry snapshots require a finite, positive tick period."));
    checkf(!active_entity_count_data_.is_empty() && !issued_unique_id_count_data_.is_empty() &&
               !cumulative_kill_count_data_.is_empty() && !active_laser_count_data_.is_empty() &&
               !cumulative_laser_spawn_count_data_.is_empty(),
           TEXT("Level telemetry must be initialised before creating a snapshot."));
    checkf(active_entity_count_data_.last_time() <= completed_tick &&
               issued_unique_id_count_data_.last_time() <= completed_tick &&
               cumulative_kill_count_data_.last_time() <= completed_tick &&
               active_laser_count_data_.last_time() <= completed_tick &&
               cumulative_laser_spawn_count_data_.last_time() <= completed_tick,
           TEXT("Level telemetry cannot contain samples after the snapshot tick."));
    checkf(contains_only_nonnegative_values(active_entity_count_data_) &&
               contains_only_nonnegative_values(cumulative_kill_count_data_) &&
               issued_unique_id_count_data_.last_value() >= 0 &&
               active_laser_count_data_.last_value() >= 0 &&
               cumulative_laser_spawn_count_data_.last_value() >= 0,
           TEXT("Level telemetry snapshots cannot contain negative counts."));

    FLevelTelemetrySnapshot snapshot;
    snapshot.elapsed_seconds = static_cast<double>(completed_tick) * tick_period;
    snapshot.tick_period = tick_period;
    snapshot.active_entities = active_entity_count_data_.last_value();
    snapshot.spawned_entities = issued_unique_id_count_data_.last_value();
    checkf(snapshot.spawned_entities >= snapshot.active_entities,
           TEXT("Spawned entity count cannot be lower than active entity count."));
    snapshot.destroyed_entities = snapshot.spawned_entities - snapshot.active_entities;
    snapshot.kills = cumulative_kill_count_data_.last_value();
    snapshot.active_lasers = active_laser_count_data_.last_value();
    snapshot.lasers_fired = cumulative_laser_spawn_count_data_.last_value();
    snapshot.active_entity_count_data =
        snapshot_with_terminal_sample(active_entity_count_data_, completed_tick);
    snapshot.cumulative_kill_count_data =
        snapshot_with_terminal_sample(cumulative_kill_count_data_, completed_tick);
    return snapshot;
}

void FLevelTelemetryManager::sample_active_entity_counts(
    tick_type const tick, FTestEntityRegistry const& entity_registry) {
    auto const active_entity_counts{entity_registry.count_alive_per_team_and_type()};
    auto const active_entity_count{sum(active_entity_counts)};
    if (active_entity_count_data_.is_empty() ||
        active_entity_count_data_.last_value() != active_entity_count) {
        active_entity_count_data_.add(tick, active_entity_count);
    }

    if (!active_entity_counts_data_.is_empty() &&
        are_equal(active_entity_counts_data_.last_value(), active_entity_counts)) {
        return;
    }

    active_entity_counts_data_.add(tick, active_entity_counts);
}

void FLevelTelemetryManager::sample_cumulative_kill_count(
    tick_type const tick, FTestEntityRegistry const& entity_registry) {
    auto const cumulative_kill_count{entity_registry.count_kills()};
    if (!cumulative_kill_count_data_.is_empty() &&
        cumulative_kill_count_data_.last_value() == cumulative_kill_count) {
        return;
    }

    cumulative_kill_count_data_.add(tick, cumulative_kill_count);
}

void
    FLevelTelemetryManager::sample_registry_slot_count(tick_type const tick,
                                                       FTestEntityRegistry const& entity_registry) {
    auto const registry_slot_count{entity_registry.get_num_elements()};
    if (!registry_slot_count_data_.is_empty() &&
        registry_slot_count_data_.last_value() == registry_slot_count) {
        return;
    }

    registry_slot_count_data_.add(tick, registry_slot_count);
}

void FLevelTelemetryManager::sample_issued_unique_id_count(
    tick_type const tick, FTestEntityRegistry const& entity_registry) {
    auto const issued_unique_id_count{entity_registry.get_num_unique_ids_issued()};
    if (!issued_unique_id_count_data_.is_empty() &&
        issued_unique_id_count_data_.last_value() == issued_unique_id_count) {
        return;
    }

    issued_unique_id_count_data_.add(tick, issued_unique_id_count);
}

void FLevelTelemetryManager::sample_laser_counts(tick_type const tick,
                                                 FLevelLaserTelemetry const lasers) {
    auto const active_laser_count{lasers.active_count};
    if (active_laser_count_data_.is_empty() ||
        active_laser_count_data_.last_value() != active_laser_count) {
        active_laser_count_data_.add(tick, active_laser_count);
    }

    auto const cumulative_laser_spawn_count{lasers.cumulative_spawn_count};
    if (cumulative_laser_spawn_count_data_.is_empty() ||
        cumulative_laser_spawn_count_data_.last_value() != cumulative_laser_spawn_count) {
        cumulative_laser_spawn_count_data_.add(tick, cumulative_laser_spawn_count);
    }
}
