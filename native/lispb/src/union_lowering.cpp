#include "declaration_emission.h"
#include "lowering.h"
#include "lowering_context.h"
#include "lowering_utils.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/node_id.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/ast/record_kind.h>
#include <codegen/schema/child_occurrences.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/union_schemas.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace lispb::codegen::detail {
namespace {

auto union_node(LoweringContext& ctx, DeclarationHandle schema) -> NodeId {
    auto& ir{ctx.ir};
    auto const& unions{ctx.declarations.unions};
    auto const& schemas{ctx.declarations.child_occurrences};
    auto const row{unions.index(schema)};
    NodeListBuilder alternatives{ir};
    for (auto const alternative : ctx.declarations.items(unions.alternatives[row])) {
        auto const alternative_row{schemas.index(alternative)};
        alternatives.add(
            ir.instance_member(lower_member_type(ctx, alternative), schemas.name[alternative_row]));
    }
    auto const created_record{
        ir.record(unions.name[row], alternatives.build(), RecordKind::union_)};
    ir.set_record_export_specifier(created_record, unions.export_specifier[row]);
    return created_record;
}

} // namespace

auto lower_union(LoweringContext& ctx, DeclarationHandle schema) -> DeclarationEmission {
    return {.header = {union_node(ctx, schema)}};
}

} // namespace lispb::codegen::detail
