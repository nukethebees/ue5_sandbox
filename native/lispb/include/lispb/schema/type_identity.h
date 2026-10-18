#pragma once

#include <lispb/indices.h>
#include <lispb/schema/type_origin.h>

#include <compare>

namespace lispb::schema {
struct TypeIdentity {
    TypeOrigin origin{TypeOrigin::declaration};
    IdentifierId module_name;
    IdentifierId namespace_name;
    IdentifierId name;

    auto operator<=>(TypeIdentity const&) const = default;
};
}
