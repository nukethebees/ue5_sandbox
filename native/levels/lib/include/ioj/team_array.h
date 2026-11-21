#pragma once

#include <ioj/enum_array.h>
#include <ioj/levels/team.h>

#include <cstdint>
#include <limits>

namespace ioj {
using TeamCount = std::uint8_t;
static_assert(enum_count<Team>() <= std::numeric_limits<TeamCount>::max());

template <typename T>
using TeamArray = EnumArray<T, Team>;
}
