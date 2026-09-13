#pragma once

#include <sandbox/core/fixed_array.h>
#include <sandbox/simulation/entity_types.h>

namespace ml::simulation {
using TeamList = ml::FixedArray<Team, static_cast<std::int32_t>(Team::COUNT)>;
}
