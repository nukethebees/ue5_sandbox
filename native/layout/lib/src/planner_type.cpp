#include <ioj/layout/planner_type.hpp>

#include <lispb/strings.h>

namespace ioj::layout {
auto integer_source_reference(lispb::schema::TypeGraph const& types,
                              lispb::schema::TypeId const node) -> std::string {
    return types.origin(node) == lispb::schema::TypeOrigin::registered_external
             ? "@" + std::string{types.strings().view(types.name(node))}
             : std::string{types.strings().view(types.cpp_spelling(node))};
}
}
