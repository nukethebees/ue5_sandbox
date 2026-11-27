#pragma once
#include <codegen/ast/indices.h>
#include <codegen/schema/declaration_handle.h>
#include <lispb/indices.h>

#include <string>
#include <vector>

namespace lispb::codegen::detail {

struct LoweringContext;

struct PhysicalColumn {
    std::string name;
    CppTypeId type;
};

struct SingleAllocationModel {
    DeclarationHandle schema;
    std::vector<PhysicalColumn> columns;
};

auto schema_namespace(LoweringContext& ctx, DeclarationHandle schema) -> std::string;

auto build_single_allocation_model(LoweringContext& ctx, DeclarationHandle schema)
    -> SingleAllocationModel;

} // namespace lispb::codegen::detail
