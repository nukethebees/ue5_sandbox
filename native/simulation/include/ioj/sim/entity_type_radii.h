#pragma once

#include "ioj/sim/entity_type.h"

#include "sandbox/core/enum_array.h"

#include <cstddef>

namespace ioj::sim {
using EntityTypeRadii =
    ml::EnumArray<EntityType, float, static_cast<std::size_t>(EntityType::COUNT)>;
} // namespace ioj::sim
