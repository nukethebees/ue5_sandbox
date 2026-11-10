#pragma once

#include <ioj/enum_array.h>
#include <ioj/sim/entities/entity_type.h>
#include <ioj/team_array.h>

#include <cstdint>

namespace ioj::sim {
template <typename T>
using TeamEntityArray = EnumArray<T, Team, EntityType>;

using EntityCounts = TeamEntityArray<std::int32_t>;
using TeamCounts = TeamArray<std::int32_t>;
}
