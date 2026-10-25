#pragma once

#include <ioj/settings/radar.h>
#include <ioj/vector.h>

namespace ioj::presentation {
auto sanitize_radar_settings(settings::Radar settings) -> settings::Radar;

namespace radar {
auto curve_parameter(settings::Radar const& settings) -> float;
auto map_distance(float normalized_distance, float curve) -> float;
auto to_radar_display_radius(float world_distance, settings::Radar const& settings) -> float;
auto to_radar_position(Vector3f local_delta, settings::Radar const& settings, float curve)
    -> Vector3f;
auto to_radar_position(Vector3f local_delta, settings::Radar const& settings) -> Vector3f;
}
}
