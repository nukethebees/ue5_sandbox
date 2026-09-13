#include "sandbox/simulation/combat/OverlapHandler.h"
#include <cassert>

#include <sandbox/simulation/entities/TestEntityRegistry.h>

namespace ml {
FOverlapHandler::FOverlapHandler(FTestEntityRegistry& registry,
                                 FOverlapResponseConfig const& config) noexcept
    : registry_{registry}
    , damage_per_overlap_detection_{config.damage_per_overlap_detection} {
    assert(damage_per_overlap_detection_ > 0);
}

void FOverlapHandler::handle(ioj::FDetectedOverlapsView const overlaps) {
    overlaps.entity_entity_overlaps.validate_array_sizes();
    overlaps.entity_static_overlaps.validate_array_sizes();

    damage_events_.reset();
    damage_events_.reserve(overlaps.entity_entity_overlaps.num() * 2 +
                           overlaps.entity_static_overlaps.num());

    auto const entity_pair_count{overlaps.entity_entity_overlaps.num()};
    for (std::int32_t index{}; index < entity_pair_count; ++index) {
        append_damage(overlaps.entity_entity_overlaps.first_entities[index]);
        append_damage(overlaps.entity_entity_overlaps.second_entities[index]);
    }

    auto const static_overlap_count{overlaps.entity_static_overlaps.num()};
    for (std::int32_t index{}; index < static_overlap_count; ++index) {
        append_damage(overlaps.entity_static_overlaps.entities[index]);
    }

    if (!damage_events_.is_empty()) {
        registry_.queue_direct_damage_events(damage_events_);
    }
}

void FOverlapHandler::append_damage(FRegistryEntityHandle const entity) {
    if (!registry_.is_valid_alive(entity)) {
        return;
    }

    switch (registry_.get_entity_type(entity)) {
        case ml::simulation::EntityType::PlayerShip:
        case ml::simulation::EntityType::Turret:
        case ml::simulation::EntityType::CapitalShip:
        case ml::simulation::EntityType::CapitalShipFighter: {
            damage_events_.add(entity, damage_per_overlap_detection_, {});
            break;
        }
        case ml::simulation::EntityType::TubeSpinner:
        case ml::simulation::EntityType::COUNT: {
            break;
        }
    }
}
}
