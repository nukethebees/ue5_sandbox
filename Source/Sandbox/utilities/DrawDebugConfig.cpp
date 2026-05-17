#include "DrawDebugConfig.h"

#include "Sandbox/logging/SandboxLogCategories.h"

#include <DrawDebugHelpers.h>

template <typename T>
auto FDrawDebugConfig::get_optional(TOptional<T> value, T const& default_value) const -> T {
    return value ? value.GetValue() : default_value;
}

auto FDrawDebugConfig::get_length() const -> float {
    return length;
}
auto FDrawDebugConfig::get_length(TOptional<float> c) const -> float {
    return get_optional(c, length);
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

void FDrawDebugConfig::draw_line(FVector const& start, FVector const& end) const {
    if (!check_world_valid()) {
        return;
    }

    DrawDebugLine(world.Get(),
                  start,
                  end,
                  get_colour(line_colour),
                  persistent,
                  lifetime,
                  depth_priority,
                  get_thickness(line_thickness));
}

void FDrawDebugConfig::draw_point(FVector const& location) const {
    if (!check_world_valid()) {
        return;
    }

    DrawDebugPoint(world.Get(),
                   location,
                   point_size,
                   get_colour(point_colour),
                   persistent,
                   lifetime,
                   depth_priority);
}

void FDrawDebugConfig::draw_arrow(FVector const& start, FVector const& end) const {
    if (!check_world_valid()) {
        return;
    }

    DrawDebugDirectionalArrow(world.Get(),
                              start,
                              end,
                              arrow_size,
                              get_colour(arrow_colour),
                              persistent,
                              lifetime,
                              depth_priority,
                              get_thickness(arrow_thickness));
}

void FDrawDebugConfig::draw_circle(FTransform const& transform, float const circle_radius) const {
    if (!check_world_valid()) {
        return;
    }

    DrawDebugCircle(world.Get(),
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

void FDrawDebugConfig::draw_circle_arc(FVector const& start,
                                       float const arc_radius,
                                       float const half_angle_rad,
                                       FQuat const& rotation) const {
    if (!check_world_valid()) {
        return;
    }

    DrawDebugCircleArc(world.Get(),
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

void FDrawDebugConfig::draw_sphere(FVector const& start, float const sphere_radius) const {
    if (!check_world_valid()) {
        return;
    }

    DrawDebugSphere(world.Get(),
                    start,
                    sphere_radius,
                    get_segments(sphere_segments),
                    get_colour(sphere_colour),
                    persistent,
                    lifetime,
                    depth_priority,
                    get_thickness());
}

void FDrawDebugConfig::draw_cone(FVector const& start,
                                 FVector const& direction,
                                 float const cone_len) const {
    if (!check_world_valid()) {
        return;
    }

    DrawDebugCone(world.Get(),
                  start,
                  direction,
                  cone_len,
                  FMath::DegreesToRadians(cone_angle_half_width_deg),
                  FMath::DegreesToRadians(cone_angle_half_height_deg),
                  get_segments(cone_segments),
                  get_colour(cone_colour),
                  persistent,
                  lifetime,
                  depth_priority,
                  get_thickness(cone_thickness));
}
void FDrawDebugConfig::draw_cone(FVector const& start, FVector const& direction) const {
    if (!check_world_valid()) {
        return;
    }

    DrawDebugCone(world.Get(),
                  start,
                  direction,
                  get_length(cone_length),
                  FMath::DegreesToRadians(cone_angle_half_width_deg),
                  FMath::DegreesToRadians(cone_angle_half_height_deg),
                  get_segments(cone_segments),
                  get_colour(cone_colour),
                  persistent,
                  lifetime,
                  depth_priority,
                  get_thickness(cone_thickness));
}

auto FDrawDebugConfig::check_world_valid() const -> bool {
    if (world.IsValid()) {
        return true;
    }

    UE_LOG(LogSandboxCore, Warning, TEXT("world is invalid"));

    return false;
}
