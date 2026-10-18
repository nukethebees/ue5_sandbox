#pragma once

#include <lispb/schema/type_graph.h>

#include <string>

namespace ioj::layout {
auto integer_source_reference(lispb::schema::TypeGraph const& types,
                              lispb::schema::TypeId const node) -> std::string;
}
