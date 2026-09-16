#include <ioj/sim/levels/fighter_spawn_slot_validation.h>

#include <ioj/sim/entity_world_bounds.h>
#include <ioj/sim/rotator_math.h>
#include <ioj/sim/transform3d.h>

namespace ioj::sim::levels {
namespace {
auto intersects(collision::WorldAABB const& first,
                collision::WorldAABB const& second,
                float const first_clearance,
                float const second_clearance) noexcept -> bool {
    return first.min.X - first_clearance <= second.max.X + second_clearance &&
           first.max.X + first_clearance >= second.min.X - second_clearance &&
           first.min.Y - first_clearance <= second.max.Y + second_clearance &&
           first.max.Y + first_clearance >= second.min.Y - second_clearance &&
           first.min.Z - first_clearance <= second.max.Z + second_clearance &&
           first.max.Z + first_clearance >= second.min.Z - second_clearance;
}

auto rotate_vector(Quaternion4f const orientation, Vector3f const vector) noexcept -> Vector3f {
    auto const quaternion_vector{ml::make_vector3f(orientation.X, orientation.Y, orientation.Z)};
    auto const twice_cross{HMM_MulV3F(HMM_Cross(quaternion_vector, vector), 2.0f)};
    return HMM_AddV3(HMM_AddV3(vector, HMM_MulV3F(twice_cross, orientation.W)),
                     HMM_Cross(quaternion_vector, twice_cross));
}
}

auto validate_fighter_spawn_slots(CapitalShipSimConfig const& capital_config,
                                  FighterSimConfig const& fighter_config,
                                  collision::EntityAABBs const& entity_bounds)
    -> std::vector<FighterSpawnSlotValidationError> {
    auto const capital_bounds{
        collision::make_entity_world_bounds(entity_bounds,
                                            collision::EntityAABBs::capital_ship_index,
                                            {},
                                            ml::make_quaternion4f(0.0f, 0.0f, 0.0f, 1.0f))};
    auto const clearance{fighter_config.avoidance_clearance_buffer};
    auto const& slots{capital_config.fighter_spawn_slots_relative_transforms};
    std::vector<collision::WorldAABB> fighter_bounds;
    fighter_bounds.reserve(slots.size());
    std::vector<FighterSpawnSlotValidationError> errors;

    for (std::size_t index{}; index < slots.size(); ++index) {
        auto const& slot{slots[index]};
        fighter_bounds.push_back(collision::make_entity_world_bounds(
            entity_bounds,
            collision::EntityAABBs::fighter_index,
            to_float(slot.location),
            to_quaternion(to_float(to_rotator(slot.rotation)))));
        if (intersects(capital_bounds, fighter_bounds.back(), 0.0f, clearance)) {
            errors.push_back({.kind = FighterSpawnSlotValidationErrorKind::IntersectsCapital,
                              .first_slot = static_cast<std::int32_t>(index),
                              .clearance = clearance});
        }
    }
    for (std::size_t first{}; first < fighter_bounds.size(); ++first) {
        for (std::size_t second{first + 1}; second < fighter_bounds.size(); ++second) {
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

auto validate_world_fighter_spawn_slots(LevelSimInitData const& data)
    -> std::vector<FighterSpawnSlotValidationError> {
    std::vector<FighterSpawnSlotValidationError> errors;
    auto const clearance{data.fighters.avoidance_clearance_buffer};
    auto const& slots{data.capital_ships.fighter_spawn_slots_relative_transforms};
    auto const validate{[&](Vectors3fConstView const locations,
                            Rotators3fConstView const rotations) {
        auto const count{locations.num()};
        for (std::int32_t capital_index{}; capital_index < count; ++capital_index) {
            auto const capital_position{locations[capital_index]};
            auto const capital_rotation{rotations[capital_index]};
            auto const capital_orientation{to_quaternion(capital_rotation)};
            auto const capital_bounds{
                collision::make_entity_world_bounds(data.entity_bounds,
                                                    collision::EntityAABBs::capital_ship_index,
                                                    capital_position,
                                                    capital_orientation)};
            for (std::size_t slot_index{}; slot_index < slots.size(); ++slot_index) {
                auto const& slot{slots[slot_index]};
                auto const local_position{
                    rotate_vector(capital_orientation, to_float(slot.location))};
                auto const fighter_position{
                    ml::make_vector3f(capital_position.X + local_position.X,
                                      capital_position.Y + local_position.Y,
                                      capital_position.Z + local_position.Z)};
                auto const fighter_orientation{capital_orientation *
                                               to_quaternion(to_float(to_rotator(slot.rotation)))};
                auto const fighter_bounds{
                    collision::make_entity_world_bounds(data.entity_bounds,
                                                        collision::EntityAABBs::fighter_index,
                                                        fighter_position,
                                                        fighter_orientation)};
                if (intersects(capital_bounds, fighter_bounds, 0.0f, clearance)) {
                    errors.push_back(
                        {.kind = FighterSpawnSlotValidationErrorKind::WorldIntersectsCapital,
                         .first_slot = static_cast<std::int32_t>(slot_index),
                         .clearance = clearance,
                         .capital_position = capital_position,
                         .capital_rotation = capital_rotation});
                }
            }
        }
    }};
    auto const initial_capitals{
        data.level_events.initial_spawns.capital_spawns.get_const_view().columns()};
    validate(initial_capitals.locations, initial_capitals.rotations);
    auto const scheduled_capitals{
        data.level_events.schedule.capital_spawns.get_const_view().columns()};
    validate(scheduled_capitals.locations, scheduled_capitals.rotations);
    return errors;
}
} // namespace ioj::sim::levels
