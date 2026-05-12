#include "DrawDebugConfig.h"

#include <DrawDebugHelpers.h>

void FDrawDebugConfig::draw_line(UWorld const* world,
                                 FVector const& start,
                                 FVector const& end) const {
    DrawDebugLine(world,
                  start,
                  end,
                  get_line_colour(),
                  persistent,
                  depth_priority,
                  lifetime,
                  get_line_thickness());
}
void FDrawDebugConfig::draw_circle_arc(UWorld const* world,
                                       FVector const& start,
                                       float const arc_radius,
                                       float const half_angle_rad) const {
    DrawDebugCircleArc(world,
                       start,
                       arc_radius,
                       FQuat::Identity,
                       half_angle_rad,
                       segments,
                       get_circle_arc_colour(),
                       persistent,
                       lifetime,
                       depth_priority,
                       get_circle_arc_thickness());
}

auto FDrawDebugConfig::get_line_thickness() const -> float {
    if (line_thickness.IsSet()) {
        return line_thickness.GetValue();
    }
    return thickness;
}
auto FDrawDebugConfig::get_line_colour() const -> FColor {
    return colour;
}

auto FDrawDebugConfig::get_circle_arc_colour() const -> FColor {
    return colour;
}
auto FDrawDebugConfig::get_circle_arc_thickness() const -> float {
    return thickness;
}
