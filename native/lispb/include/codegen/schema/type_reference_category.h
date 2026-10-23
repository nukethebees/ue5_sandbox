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
    equivalent_type,
    column,
    group,
    target,
    method,
    validation_dependency,
    setting
};

} // namespace lispb::codegen
