#pragma once

#include <lispb/schema/indices.h>

#include <optional>
#include <string>

namespace ioj::layout {

auto physical_type_spelling(lispb::schema::TypeGraph const& types, lispb::schema::TypeId type)
    -> std::optional<std::string>;
auto physical_type_spelling(lispb::schema::TypeGraph const& types,
                            lispb::schema::ResolvedOccurrenceId use) -> std::optional<std::string>;

}
