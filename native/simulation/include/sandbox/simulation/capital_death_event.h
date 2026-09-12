#pragma once

#include "sandbox/simulation/vector_types.h"

#include <cstdint>

namespace ml::simulation {
struct CapitalDeathEvent {
    Vector3f location;
    std::int32_t batch_index{};
};
} // namespace ml::simulation
