#pragma once

#include <lispb/schema/type_origin.h>

#include <compare>
#include <string>

namespace lispb::schema {
struct TypeIdentity {
    TypeOrigin origin{TypeOrigin::declaration};
    std::string module_name;
    std::string namespace_name;
    std::string name;

    auto operator<=>(TypeIdentity const&) const = default;
};
}
