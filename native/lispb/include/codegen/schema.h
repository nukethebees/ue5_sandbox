#pragma once

#include <codegen/schema/physical_type_use.h>
#include <codegen/schema/registered_type_schema.h>
#include <codegen/schema/storage_operation.h>
#include <codegen/schema/type_ref.h>

#include <string>
#include <vector>

namespace codegen {

auto resolve_type(TypeRef const& reference, TypeRegistry const& types) -> CppType;
auto resolve_type_use(TypeRef const& reference, TypeRegistry const& types) -> ResolvedCppTypeUse;
auto native_spelling(std::string const& spelling) -> std::string;
auto all_storage_operations() -> std::vector<StorageOperation>;

} // namespace codegen
