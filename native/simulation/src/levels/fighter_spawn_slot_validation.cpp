#include <ioj/sim/levels/fighter_spawn_slot_validation.h>

#include <ioj/sim/entity_world_bounds.h>
#include <ioj/sim/rotator_math.h>
#include <ioj/sim/transform3d.h>

namespace ioj::levels {
namespace {
auto intersects(sim::collision::WorldAABB const& first,
                sim::collision::WorldAABB const& second,
                float const first_clearance,
                float const second_clearance) noexcept -> bool {
    return first.min.X - first_clearance <= second.max.X + second_clearance &&
           first.max.X + first_clearance >= second.min.X - second_clearance &&
           first.min.Y - first_clearance <= second.max.Y + second_clearance &&
           first.max.Y + first_clearance >= second.min.Y - second_clearance &&
           first.min.Z - first_clearance <= second.max.Z + second_clearance &&
           first.max.Z + first_clearance >= second.min.Z - second_clearance;
}

}

auto validate_fighter_spawn_slots(sim::CapitalShipSimConfig const& capital_config,
                                  sim::FighterSimConfig const& fighter_config,
                                  sim::collision::EntityAABBs const& entity_bounds)
    -> std::vector<FighterSpawnSlotValidationError> {
    auto const capital_bounds{
        sim::collision::make_entity_world_bounds(entity_bounds,
                                                 sim::EntityType::CapitalShip,
                                                 {},
                                                 ml::make_quaternion4f(0.0f, 0.0f, 0.0f, 1.0f))};
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
            sim::to_float(slot.location),
            sim::to_quaternion(sim::to_float(ioj::to_rotator(slot.rotation)))));
        if (intersects(capital_bounds, fighter_bounds.back(), 0.0f, clearance)) {
            errors.push_back({.kind = FighterSpawnSlotValidationErrorKind::IntersectsCapital,
                              .first_slot = static_cast<std::int32_t>(index),
                              .clearance = clearance});
        }
    }
    auto const fighter_bound_count{fighter_bounds.size()};
    for (std::size_t first{}; first < fighter_bound_count; ++first) {
        for (std::size_t second{first + 1}; second < fighter_bound_count; ++second) {
            if (intersects(fighter_bounds[first], fighter_bounds[second], clearance, clearance)) {
                errors.push_back({.kind = FighterSpawnSlotValidationErrorKind::SlotsOverlap,
                                  .first_slot = static_cast<std::int32_t>(first),
                                  .second_slot = static_cast<std::int32_t>(second),
                                  .clearance = clearance});
            }
        }
    }
    return errors;
}

} // namespace ioj::levels
