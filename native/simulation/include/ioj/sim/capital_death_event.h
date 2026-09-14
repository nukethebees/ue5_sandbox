#pragma once

#include "ioj/sim/vector_types.h"

#include <cstdint>

namespace ioj::sim {
struct CapitalDeathEvent {
    Vector3f location;
    std::int32_t batch_index{};
};
} // namespace ioj::sim
