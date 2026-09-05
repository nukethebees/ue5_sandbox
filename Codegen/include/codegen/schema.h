#pragma once

#include <codegen/schema/enum_conversion.h>
#include <codegen/schema/enum_module_schema.h>
#include <codegen/schema/enum_reflection.h>
#include <codegen/schema/enum_schema.h>
#include <codegen/schema/enumerator_schema.h>
#include <codegen/schema/facade_method_schema.h>
#include <codegen/schema/facade_module_schema.h>
#include <codegen/schema/facade_schema.h>
#include <codegen/schema/fixed_soa_schema.h>
#include <codegen/schema/function_schema.h>
#include <codegen/schema/homogeneous_layout_schema.h>
#include <codegen/schema/homogeneous_module_schema.h>
#include <codegen/schema/homogeneous_value_schema.h>
#include <codegen/schema/manifest.h>
#include <codegen/schema/module_schema.h>
#include <codegen/schema/module_settings.h>
#include <codegen/schema/parameter_schema.h>
#include <codegen/schema/schema_version.h>
#include <codegen/schema/settings_module_schema.h>
#include <codegen/schema/soa_member_kind.h>
#include <codegen/schema/soa_member_schema.h>
#include <codegen/schema/soa_module_schema.h>
#include <codegen/schema/soa_schema.h>
#include <codegen/schema/static_table_column_schema.h>
#include <codegen/schema/static_table_group_schema.h>
#include <codegen/schema/static_table_module_schema.h>
#include <codegen/schema/static_table_row_schema.h>
#include <codegen/schema/static_table_schema.h>
#include <codegen/schema/storage_operation.h>
#include <codegen/schema/type_ref.h>
#include <codegen/schema/umbrella_module_schema.h>
#include <codegen/schema/vector_module_schema.h>

#include <map>
#include <string>
#include <vector>

namespace codegen {

auto resolve_type(TypeRef const& reference, std::map<std::string, CppType> const& types)
    -> CppType;
auto all_storage_operations() -> std::vector<StorageOperation>;

} // namespace codegen
