#include "LevelTelemetryManager.h"

#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>

void FLevelTelemetryManager::initialise(FTestEntityRegistry const& entity_registry) {
    reset();
    sample_active_entity_count(0, entity_registry);
}

void FLevelTelemetryManager::reset() {
    active_entity_count_data_.reset();
}

void FLevelTelemetryManager::tick(tick_type const tick,
                                  FTestEntityRegistry const& entity_registry) {
    sample_active_entity_count(tick, entity_registry);
}

void
    FLevelTelemetryManager::sample_active_entity_count(tick_type const tick,
                                                       FTestEntityRegistry const& entity_registry) {
    auto const active_entity_count{entity_registry.get_num_alive_active_entities()};
    if (!active_entity_count_data_.is_empty() &&
        active_entity_count_data_.last_value() == active_entity_count) {
        return;
    }

    active_entity_count_data_.add(tick, active_entity_count);
}
