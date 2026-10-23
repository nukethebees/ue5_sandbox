#pragma once
#include <codegen/lowering/indices.h>
#include <codegen/schema/declaration_handle.h>
#include <lispb/indices.h>

#include <string>

namespace lispb::codegen::detail {

struct LoweringContext;

auto schema_namespace(LoweringContext& context, DeclarationHandle schema) -> std::string;
auto is_strided_schema(LoweringContext& context, DeclarationHandle schema) -> bool;

auto build_single_allocation_model(LoweringContext& context, DeclarationHandle schema)
    -> SingleAllocationModelId;

auto column_for(LoweringContext& context, SingleAllocationModelId model, LoweringPathId path)
    -> SingleAllocationColumnId;
} // namespace lispb::codegen::detail
