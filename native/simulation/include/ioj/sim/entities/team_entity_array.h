#pragma once

#include <ioj/enum_array.h>
#include <ioj/sim/entities/entity_type.h>
#include <ioj/team_array.h>

namespace ioj::sim {
template <typename T>
using TeamEntityArray = TeamArray<EnumArray<EntityType, T>>;
}
