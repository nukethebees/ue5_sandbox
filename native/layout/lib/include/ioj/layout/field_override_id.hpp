#pragma once

#include <lispb/schema/indices.h>

#include <compare>
#include <string>

namespace ioj::layout {
struct FieldOverrideId {
    lispb::schema::TypeId type;
    std::string field_name;
    auto operator<=>(FieldOverrideId const&) const = default;
};
}
