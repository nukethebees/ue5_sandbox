#pragma once

#include "ioj/enum_array.h"
#include "ioj/sim/entity_type.h"

#include <cstddef>

namespace ioj::sim {
using EntityTypeRadii = ml::EnumArray<EntityType, float>;
} // namespace ioj::sim
