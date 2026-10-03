#include "ioj/sim/overlap_handler.h"

#include <ioj/sim/combat_events.h>

#include <cassert>

namespace ioj::sim {
OverlapHandler::OverlapHandler(CombatEvents& events, OverlapResponseConfig const& config) noexcept
    : events_{events}
    , damage_per_overlap_detection_{config.damage_per_overlap_detection} {
    assert(damage_per_overlap_detection_ > 0);
}

void OverlapHandler::handle(collision::DetectedOverlapsView const overlaps) {
    overlaps.entity_entity_overlaps.validate();
    overlaps.entity_static_overlaps.validate();

    damage_events_.reset();
    damage_events_.reserve(overlaps.entity_entity_overlaps.num() * 2 +
                           overlaps.entity_static_overlaps.num());

    auto const entity_pair_count{overlaps.entity_entity_overlaps.num()};
    auto const first_entities{overlaps.entity_entity_overlaps.first_entities()};
    auto const second_entities{overlaps.entity_entity_overlaps.second_entities()};
    for (std::uint32_t index{}; index < entity_pair_count; ++index) {
        append_damage(first_entities[index]);
        append_damage(second_entities[index]);
    }

    auto const static_overlap_count{overlaps.entity_static_overlaps.num()};
    auto const static_entities{overlaps.entity_static_overlaps.entities()};
    for (std::uint32_t index{}; index < static_overlap_count; ++index) {
        append_damage(static_entities[index]);
    }

    if (!damage_events_.is_empty()) {
        events_.queue_damage(damage_events_);
    }
}

void OverlapHandler::append_damage(EntityUniqueId const id) {
    switch (id.entity_type()) {
        case EntityType::PlayerShip:
        case EntityType::Turret:
        case EntityType::CapitalShip:
        case EntityType::Fighter: {
            damage_events_.add(id, damage_per_overlap_detection_, {});
            break;
        }
        case EntityType::TubeSpinner:
        case EntityType::COUNT: {
            break;
        }
    }
}
}
