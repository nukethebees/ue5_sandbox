#include <ioj/presentation/observer_camera.h>

#include <cmath>
#include <numbers>

namespace ioj::presentation {
auto make_observer_camera_transform(std::span<Vector3d const> const target_positions,
                                    Vector3d const offset_direction,
                                    double const distance)
    -> std::optional<ObserverCameraTransform> {
    if (target_positions.empty() || !is_finite(offset_direction) || !std::isfinite(distance) ||
        distance <= 0.0) {
        return std::nullopt;
    }
    auto const direction_length{
        std::hypot(offset_direction.x(), offset_direction.y(), offset_direction.z())};
    if (!std::isfinite(direction_length) || direction_length <= 1.e-4) {
        return std::nullopt;
    }

    Vector3d focus;
    auto const count{static_cast<double>(target_positions.size())};
    for (auto const position : target_positions) {
        if (!is_finite(position)) {
            return std::nullopt;
        }
        focus += position / count;
    }
    auto const direction{offset_direction / direction_length};
    auto const position{focus + direction * distance};
    if (!is_finite(focus) || !is_finite(position)) {
        return std::nullopt;
    }
    auto const forward{-direction};
    constexpr auto degrees_per_radian{180.0 / std::numbers::pi};
    Rotator3d const rotation{std::atan2(forward.z(), std::hypot(forward.x(), forward.y())) *
                                 degrees_per_radian,
                             std::atan2(forward.y(), forward.x()) * degrees_per_radian,
                             0.0};
    return ObserverCameraTransform{focus,
                                   {.rotation = to_quaternion(rotation), .location = position}};
}

auto orbit_camera_transform(Vector3d const pivot,
                            double const angle_degrees,
                            double const radius,
                            double const height) -> std::optional<Transform3d> {
    if (!is_finite(pivot) || !std::isfinite(angle_degrees) || !std::isfinite(radius) ||
        radius <= 0.0 || !std::isfinite(height)) {
        return std::nullopt;
    }
    auto const angle{std::fmod(angle_degrees, 360.0) * std::numbers::pi / 180.0};
    Vector3d const offset{std::cos(angle) * radius, std::sin(angle) * radius, height};
    auto const camera{
        make_observer_camera_transform(std::span{&pivot, 1}, offset, std::hypot(radius, height))};
    return camera ? std::optional{camera->transform} : std::nullopt;
}
}
