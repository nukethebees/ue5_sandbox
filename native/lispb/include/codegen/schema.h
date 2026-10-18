#pragma once

#include <codegen/ast/indices.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/resolved_cpp_type_use.h>
#include <codegen/schema/storage_operation.h>

#include <string>
#include <vector>

namespace lispb::codegen {

struct TypeRegistry;

auto resolve_type(TypeRefId reference, TypeRegistry const& types) -> CppTypeId;
auto resolve_type_use(TypeRefId reference, TypeRegistry const& types) -> ResolvedCppTypeUse;
auto native_spelling(std::string const& spelling) -> std::string;
auto all_storage_operations() -> std::vector<StorageOperation>;

} // namespace lispb::codegen
