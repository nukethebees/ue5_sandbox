#pragma once

#include <ioj/vector.h>

#include <cstdint>
#include <span>

namespace ioj::presentation {
struct SampledSpeedHudView {
    std::span<Vector2d const> samples;
    std::uint32_t oldest_index{};
};
}
