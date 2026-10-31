#pragma once

#include <ioj/math_types.h>

#include <cstdint>

namespace ioj::presentation {
struct SparkBurstEmission {
    Vector3f location{};
    Vector3f direction{0.f, 0.f, 1.f};
    Vector3f colour{1.f, 1.f, 1.f};
    std::uint32_t seed{};
};
}
