#pragma once

#include <cstdint>

namespace lispb::codegen {

enum class TypeReferenceCategory : std::uint8_t {
    underlying_type,
    cpp_type,
    value,
    source,
    storage,
    field,
    member,
    function,
    const_view_function,
    view_function,
    discriminant,
    alternative,
    value_type,
    equivalent_type,
    array_allocator,
    column,
    group,
    target,
    method,
    layout_value,
    validation_dependency,
    module_allocator,
    setting
};

} // namespace lispb::codegen
