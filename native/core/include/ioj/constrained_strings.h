#pragma once

#include <ioj/constrained_value.h>

#include <string>

namespace ioj {
using NonEmptyString =
    ConstrainedValue<std::string,
                     ConstrainedValuePolicy{
                         [](std::string const& value) noexcept { return !value.empty(); },
                         "String must not be empty"}>;
}
