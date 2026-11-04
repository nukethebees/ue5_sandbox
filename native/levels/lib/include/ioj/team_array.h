#pragma once

#include <ioj/enum_array.h>
#include <ioj/levels/team.h>

namespace ioj {
template <typename T>
using TeamArray = EnumArray<T, Team>;
}
