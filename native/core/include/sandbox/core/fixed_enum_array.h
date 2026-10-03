#pragma once

#include <sandbox/core/enum_traits.h>
#include <sandbox/core/fixed_array.h>

namespace ml {
template <typename Enum>
using FixedEnumArray = FixedArray<Enum, enum_count<Enum>()>;
}
