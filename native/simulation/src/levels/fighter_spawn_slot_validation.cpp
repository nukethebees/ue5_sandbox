#include <ioj/sim/levels/fighter_spawn_slot_validation.h>

#include <ioj/rotator_math.h>
#include <ioj/sim/collision/entity_world_bounds.h>
#include <ioj/transform3d.h>

namespace ioj::levels {
namespace {
auto intersects(sim::collision::WorldAABB const& first,
                sim::collision::WorldAABB const& second,
                float const first_clearance,
                float const second_clearance) noexcept -> bool {
    return first.min.x() - first_clearance <= second.max.x() + second_clearance &&
           first.max.x() + first_clearance >= second.min.x() - second_clearance &&
           first.min.y() - first_clearance <= second.max.y() + second_clearance &&
           first.max.y() + first_clearance >= second.min.y() - second_clearance &&
           first.min.z() - first_clearance <= second.max.z() + second_clearance &&
           first.max.z() + first_clearance >= second.min.z() - second_clearance;
}

}

auto validate_fighter_spawn_slots(sim::CapitalShipSimConfig const& capital_config,
                                  sim::FighterSimConfig const& fighter_config,
                                  sim::collision::EntityAABBs const& entity_bounds)
    -> std::vector<FighterSpawnSlotValidationError> {
    auto const capital_bounds{sim::collision::make_entity_world_bounds(
        entity_bounds, sim::EntityType::CapitalShip, {}, Quaternion4f{0.0f, 0.0f, 0.0f, 1.0f})};
    auto const clearance{fighter_config.avoidance_clearance_buffer};
    auto const& slots{capital_config.fighter_spawn_slots_relative_transforms};
    std::vector<sim::collision::WorldAABB> fighter_bounds;
    fighter_bounds.reserve(slots.size());
    std::vector<FighterSpawnSlotValidationError> errors;

    auto const slot_count{slots.size()};
    for (std::size_t index{}; index < slot_count; ++index) {
        auto const& slot{slots[index]};
        fighter_bounds.push_back(sim::collision::make_entity_world_bounds(
            entity_bounds,
            sim::EntityType::Fighter,
            to_float(slot.location),
            to_quaternion(to_float(to_rotator(slot.rotation)))));
        if (intersects(capital_bounds, fighter_bounds.back(), 0.0f, clearance)) {
            errors.push_back({.kind = FighterSpawnSlotValidationErrorKind::IntersectsCapital,
                              .first_slot = static_cast<FighterSpawnSlotIndex>(index),
                              .clearance = clearance});
        }
    }
    auto const fighter_bound_count{fighter_bounds.size()};
    for (std::size_t first{}; first < fighter_bound_count; ++first) {
        for (std::size_t second{first + 1}; second < fighter_bound_count; ++second) {
            if (intersects(fighter_bounds[first], fighter_bounds[second], clearance, clearance)) {
                errors.push_back({.kind = FighterSpawnSlotValidationErrorKind::SlotsOverlap,
                                  .first_slot = static_cast<FighterSpawnSlotIndex>(first),
                                  .second_slot = static_cast<FighterSpawnSlotIndex>(second),
                                  .clearance = clearance});
            }
        }
    }
    return errors;
}

} // namespace ioj::levels
