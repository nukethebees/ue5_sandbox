#pragma once

#include <lispb/indices.h>
#include <lispb/schema/indices.h>

#include <compare>

namespace ioj::layout {
// Both handles belong to the analyzed document.
struct FieldOverrideId {
    auto operator<=>(FieldOverrideId const&) const = default;

    lispb::schema::TypeId type;
    lispb::IdentifierId field_name;
};
}
