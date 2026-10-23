#pragma once

#include <lispb/indices.h>
#include <lispb/schema/type_origin.h>

#include <compare>

namespace lispb::schema {
struct TypeIdentity {
    TypeOrigin origin{TypeOrigin::declaration};
    StringId module_name;
    StringId namespace_name;
    StringId name;

    auto operator<=>(TypeIdentity const&) const = default;
};
}
