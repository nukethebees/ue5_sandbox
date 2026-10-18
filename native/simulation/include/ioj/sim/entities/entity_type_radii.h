#pragma once

#include "ioj/enum_array.h"
#include "ioj/sim/entities/entity_type.h"

#include <cstddef>

namespace ioj::sim {
using EntityTypeRadii = ioj::EnumArray<EntityType, float>;
} // namespace ioj::sim
