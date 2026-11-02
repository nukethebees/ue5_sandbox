#pragma once
#include <codegen/lowering/indices.h>
#include <codegen/schema/declaration_handle.h>
#include <lispb/indices.h>

#include <string>

namespace lispb::codegen::detail {

struct LoweringContext;

auto schema_namespace(LoweringContext& ctx, DeclarationHandle schema) -> std::string;
auto is_strided_schema(LoweringContext& ctx, DeclarationHandle schema) -> bool;

auto build_single_allocation_model(LoweringContext& ctx, DeclarationHandle schema)
    -> SingleAllocationModelId;

auto column_for(LoweringContext& ctx, SingleAllocationModelId model, LoweringPathId path)
    -> SingleAllocationColumnId;
} // namespace lispb::codegen::detail
