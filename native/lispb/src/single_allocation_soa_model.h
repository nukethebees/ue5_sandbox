#pragma once
#include <codegen/lowering/indices.h>
#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/soa_backend.h>
#include <lispb/indices.h>

#include <map>

namespace lispb::codegen::detail {

struct LoweringContext;

auto build_single_allocation_model(LoweringContext& context,
                                   DeclarationHandle schema,
                                   std::map<IdentifierId, DeclarationHandle> const& schemas,
                                   SoaBackend backend) -> SingleAllocationModelId;

auto column_for(LoweringContext& context, SingleAllocationModelId model, LoweringPathId path)
    -> SingleAllocationColumnId;
auto compact_vector_for(LoweringContext& context,
                        SingleAllocationModelId model,
                        LoweringPathId path) -> CompactVectorId;
} // namespace lispb::codegen::detail
