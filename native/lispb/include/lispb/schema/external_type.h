#pragma once

#include <codegen/ast/cpp_type.h>
#include <codegen/schema/registered_type_schema.h>
#include <lispb/schema/integer_scalar_type.h>

#include <string>
#include <variant>
#include <vector>

namespace lispb::schema {
struct ExternalType {
    codegen::CppType cpp_type;
    std::vector<std::string> registered_names;
    std::variant<std::monostate, IntegerScalarType, codegen::FloatingPointFormat> semantics;
};
}
