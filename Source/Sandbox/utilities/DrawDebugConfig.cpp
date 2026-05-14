#include "DrawDebugConfig.h"

#include <DrawDebugHelpers.h>

template <typename T>
auto FDrawDebugConfig::get_optional(TOptional<T> value, T const& default_value) const -> T {
    return value ? value.GetValue() : default_value;
}

auto FDrawDebugConfig::get_colour() const -> FColor {
    return colour;
}
auto FDrawDebugConfig::get_colour(TOptional<FColor> value) const -> FColor {
    return get_optional(value, colour);
}
auto FDrawDebugConfig::get_thickness() const -> float {
    return thickness;
}
auto FDrawDebugConfig::get_thickness(TOptional<float> value) const -> float {
    return get_optional(value, thickness);
}
auto FDrawDebugConfig::get_segments() const -> int32 {
    return segments;
}
auto FDrawDebugConfig::get_segments(TOptional<int32> t) const -> int32 {
    return get_optional(t, segments);
}

void FDrawDebugConfig::draw_line(UWorld const* world,
                                 FVector const& start,
                                 FVector const& end) const {
    DrawDebugLine(world,
                  start,
                  end,
                  get_colour(line_colour),
                  persistent,
                  lifetime,
                  depth_priority,
                  get_thickness(line_thickness));
}

void FDrawDebugConfig::draw_point(UWorld const* world, FVector const& location) const {
    DrawDebugPoint(world,
                   location,
                   point_size,
                   get_colour(point_colour),
                   persistent,
                   lifetime,
                   depth_priority);
}

void FDrawDebugConfig::draw_circle(UWorld const* world,
                                   FTransform const& transform,
                                   float const circle_radius) const {
    DrawDebugCircle(world,
                    transform.ToMatrixWithScale(),
                    circle_radius,
                    get_segments(circle_segments),
                    get_colour(circle_colour),
                    persistent,
                    lifetime,
                    depth_priority,
                    get_thickness(circle_thickness),
                    circle_draw_axis);
}
auto FDrawDebugConfig::get_circle_xy_rotation() const -> FRotator {
    return FRotator{90.f, 0.f, 0.f};
}

void FDrawDebugConfig::draw_circle_arc(UWorld const* world,
                                       FVector const& start,
                                       float const arc_radius,
                                       float const half_angle_rad,
                                       FQuat const& rotation) const {
    DrawDebugCircleArc(world,
                       start,
                       arc_radius,
                       rotation,
                       half_angle_rad,
                       get_segments(circle_arc_segments),
                       get_colour(circle_arc_colour),
                       persistent,
                       lifetime,
                       depth_priority,
                       get_thickness(circle_arc_thickness));
}
