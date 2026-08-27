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
}

void FLevelTelemetryManager::initialise(FTestEntityRegistry const& entity_registry) {
    reset();
    sample_active_entity_counts(0, entity_registry);
    sample_cumulative_kill_count(0, entity_registry);
    sample_registry_slot_count(0, entity_registry);
    sample_issued_unique_id_count(0, entity_registry);
}

void FLevelTelemetryManager::reset() {
    active_entity_count_data_.reset();
    active_entity_counts_data_.reset();
    cumulative_kill_count_data_.reset();
    registry_slot_count_data_.reset();
    issued_unique_id_count_data_.reset();
}

void FLevelTelemetryManager::tick(tick_type const tick,
                                  FTestEntityRegistry const& entity_registry) {
    sample_active_entity_counts(tick, entity_registry);
    sample_cumulative_kill_count(tick, entity_registry);
    sample_registry_slot_count(tick, entity_registry);
    sample_issued_unique_id_count(tick, entity_registry);
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

void FLevelTelemetryManager::sample_registry_slot_count(
    tick_type const tick, FTestEntityRegistry const& entity_registry) {
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
