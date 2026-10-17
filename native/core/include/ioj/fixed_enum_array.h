#pragma once

#include <ioj/enum_traits.h>
#include <ioj/fixed_array.h>

namespace ml {
template <typename Enum>
using FixedEnumArray = FixedArray<Enum, enum_count<Enum>()>;
}
