#pragma once

#include <ioj/frame_array.h>

#include <cstdint>
#include <span>

namespace ioj::sim::lasers {
struct Sim;
}

namespace ioj::sim::spinners {
struct ThinkingInputs {
    FrameArray<std::uint32_t>& pending_fire_indices;
};

struct ActionInputs {
    lasers::Sim& laser_simulation;
    std::span<std::uint32_t const> pending_fire_indices{};
};
} // namespace ioj::sim::spinners
