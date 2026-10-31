#pragma once

#include <ioj/presentation/spark_scalar_range.h>

#include <cstdint>

namespace ioj::presentation {
struct SparkBurstStyle {
    std::int32_t count{16};
    SparkScalarRange speed{3000.f, 10000.f};
    SparkScalarRange lifetime{0.08f, 0.25f};
    SparkScalarRange size{4.f, 12.f};
    float intensity{20.f};
    float spread_angle_degrees{90.f};
    float streak_time{0.025f};
};
}
