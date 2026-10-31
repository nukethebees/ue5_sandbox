#pragma once

#include <ioj/math_types.h>

#include <cstdint>

namespace ioj::presentation {
struct SparkRendererSettings {
    std::int32_t capacity{100000};
    float maximum_draw_distance{200000.f};
    Vector3f acceleration{0.f, 0.f, -980.f};
    float minimum_thickness_pixels{0.75f};
    float maximum_thickness_pixels{12.f};
    float maximum_length_pixels{128.f};
};

auto normalize_spark_renderer_settings(SparkRendererSettings settings) -> SparkRendererSettings;
}
