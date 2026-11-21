#pragma once

#include <ioj/rotator_math.h>
#include <ioj/sim/collision/entity_world_bounds.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ioj::sim {
struct FighterLaunchColumn {
    auto position(std::uint32_t index) const -> Vector3f {
        return {first.x(), first.y(), first.z() + static_cast<float>(index) * spacing};
    }

    Vector3f first;
    float spacing;
    Rotator3f heading;
};

inline auto fighter_launch_column(collision::EntityAABBs const& bounds,
                                  Vector3f pivot,
                                  Rotator3f rotation,
                                  float clearance_buffer) -> FighterLaunchColumn {
    auto const top{collision::make_entity_world_bounds(
                       bounds, EntityType::CapitalShip, pivot, to_quaternion(rotation))
                       .max.z()};
    auto const fighter_height{std::abs(bounds.centre(EntityType::Fighter).z()) +
                              bounds.half_extents(EntityType::Fighter).z()};
    auto const clearance{
        std::max(collision::get_entity_radius(bounds, EntityType::Fighter), fighter_height) +
        clearance_buffer};
    constexpr float gap_cm{1.f};
    return {{pivot.x(), pivot.y(), top + clearance + gap_cm},
            2.f * clearance + gap_cm,
            {0.f, rotation.yaw + 90.f, 0.f}};
}
}
