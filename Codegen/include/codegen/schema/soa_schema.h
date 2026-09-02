#pragma once

#include <codegen/schema/fixed_soa_schema.h>
#include <codegen/schema/function_schema.h>
#include <codegen/schema/soa_member_schema.h>
#include <codegen/schema/storage_operation.h>
#include <codegen/schema/type_ref.h>

#include <optional>
#include <string>
#include <vector>

namespace codegen {

struct SoaSchema {
    std::string name;
    std::optional<std::string> view_name;
    std::optional<std::string> const_view_name;
    std::vector<SoaMemberSchema> members;
    std::vector<StorageOperation> operations;
    std::optional<std::string> export_specifier;
    std::vector<FunctionSchema> functions;
    std::vector<FunctionSchema> mutable_view_functions;
    std::vector<std::string> using_declarations;
    std::optional<TypeRef> equivalent_type;
    bool copy_element_memberwise{false};
    std::optional<FixedSoaSchema> fixed;
};

} // namespace codegen
