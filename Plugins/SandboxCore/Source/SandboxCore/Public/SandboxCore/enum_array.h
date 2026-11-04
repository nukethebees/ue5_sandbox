#pragma once

#include <ioj/enum_array.h>

template <typename T, typename... Enums>
using TEnumArray = ioj::EnumArray<T, Enums...>;
