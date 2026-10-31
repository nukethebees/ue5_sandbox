#pragma once

#include <ioj/transform3d.h>

#include <optional>
#include <span>

namespace ioj::presentation {
struct ObserverCameraTransform {
    Vector3d focus;
    Transform3d transform;
};

// Invalid or empty targets, a degenerate offset, or nonpositive distance have no camera.
auto make_observer_camera_transform(std::span<Vector3d const> target_positions,
                                    Vector3d offset_direction,
                                    double distance) -> std::optional<ObserverCameraTransform>;

auto orbit_camera_transform(Vector3d pivot, double angle_degrees, double radius, double height)
    -> std::optional<Transform3d>;
}
