#pragma once
#include <codegen/lowering/indices.h>
#include <codegen/schema/declaration_handle.h>
#include <lispb/indices.h>

namespace lispb::codegen::detail {

struct LoweringContext;

auto build_single_allocation_model(LoweringContext& context, DeclarationHandle schema)
    -> SingleAllocationModelId;

auto column_for(LoweringContext& context, SingleAllocationModelId model, LoweringPathId path)
    -> SingleAllocationColumnId;
} // namespace lispb::codegen::detail
