#pragma once

#include <ioj/constrained_value.h>

namespace codegen_compile_fixture {
inline constexpr auto range_policy{ioj::ConstrainedValuePolicy{
    [](auto const& value) constexpr noexcept { return value.minimum <= value.maximum; },
    "Expected minimum <= maximum"}};
}
