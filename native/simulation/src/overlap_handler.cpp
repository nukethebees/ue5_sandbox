#include "ioj/sim/overlap_handler.h"
#include <cassert>

#include <ioj/sim/agent_accessor.h>
#include <ioj/sim/entity_registry.h>

namespace ioj::sim {
OverlapHandler::OverlapHandler(EntityRegistry& registry,
                               AgentAccessor const& agents,
                               OverlapResponseConfig const& config) noexcept
    : registry_{registry}
    , agents_{agents}
    , damage_per_overlap_detection_{config.damage_per_overlap_detection} {
    assert(damage_per_overlap_detection_ > 0);
}

void OverlapHandler::handle(collision::DetectedOverlapsView const overlaps) {
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
    auto const id{registry_.get_current_id(entity)};
    if (!agents_.is_alive(id)) {
        return;
    }

    switch (id.entity_type()) {
        case EntityType::PlayerShip:
        case EntityType::Turret:
        case EntityType::CapitalShip:
        case EntityType::Fighter: {
            damage_events_.add(entity, damage_per_overlap_detection_, {});
            break;
        }
        case EntityType::TubeSpinner:
        case EntityType::COUNT: {
            break;
        }
    }
}
}
