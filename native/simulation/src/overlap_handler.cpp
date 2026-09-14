#include "ioj/sim/overlap_handler.h"
#include <cassert>

#include <ioj/sim/entity_registry.h>

namespace ioj::sim {
OverlapHandler::OverlapHandler(EntityRegistry& registry,
                               OverlapResponseConfig const& config) noexcept
    : registry_{registry}
    , damage_per_overlap_detection_{config.damage_per_overlap_detection} {
    assert(damage_per_overlap_detection_ > 0);
}

void OverlapHandler::handle(ioj::sim::collision::DetectedOverlapsView const overlaps) {
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

void OverlapHandler::append_damage(RegistryEntityHandle const entity) {
    if (!registry_.is_valid_alive(entity)) {
        return;
    }

    switch (registry_.get_entity_type(entity)) {
        case ioj::sim::EntityType::PlayerShip:
        case ioj::sim::EntityType::Turret:
        case ioj::sim::EntityType::CapitalShip:
        case ioj::sim::EntityType::Fighter: {
            damage_events_.add(entity, damage_per_overlap_detection_, {});
            break;
        }
        case ioj::sim::EntityType::TubeSpinner:
        case ioj::sim::EntityType::COUNT: {
            break;
        }
    }
}
}
