#pragma once

#include <ioj/sim/entity_types.h>
#include <sandbox/core/fixed_array.h>

namespace ioj::sim {
using TeamList = ml::FixedArray<Team, static_cast<std::int32_t>(Team::COUNT)>;
}
