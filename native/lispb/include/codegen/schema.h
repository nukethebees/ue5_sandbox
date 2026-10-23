#pragma once

#include <codegen/ast/indices.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/resolved_cpp_type_use.h>

#include <string>

namespace lispb::codegen {

struct TypeRegistry;

auto resolve_type(TypeRefId reference, TypeRegistry const& types) -> CppTypeId;
auto resolve_type_use(TypeRefId reference, TypeRegistry const& types) -> ResolvedCppTypeUse;
auto native_spelling(std::string const& spelling) -> std::string;

} // namespace lispb::codegen
