#pragma once

#include <ioj/enum_traits.h>
#include <ioj/fixed_array.h>

namespace ioj {
template <typename Enum>
using FixedEnumArray = FixedArray<Enum, enum_count<Enum>()>;
}
