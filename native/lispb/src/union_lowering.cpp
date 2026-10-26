#include "declaration_emission.h"
#include "lowering.h"
#include "lowering_context.h"

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

auto alternative_type(LoweringContext& context,
                      TypeRefId reference,
                      std::optional<std::uint64_t> count) -> CppTypeId {
    auto& ir{context.ir};
    auto element{context.resolve(reference)};
    if (!count.has_value()) {
        return element;
    }
    return ir.types.template_type(
        ir.types.named("std::array", {ir.types.dependency("std::array", "array")}),
        {element, ir.types.intern(std::to_string(*count))});
}

auto union_node(LoweringContext& context, DeclarationHandle schema) -> NodeId {
    auto& ir{context.ir};
    auto const& unions{context.declarations.unions};
    auto const& schemas{context.declarations.child_occurrences};
    auto const row{unions.index(schema)};
    NodeListBuilder alternatives{ir};
    for (auto const alternative : context.declarations.items(unions.alternatives[row])) {
        auto const alternative_row{schemas.index(alternative)};
        alternatives.add(ir.instance_member(alternative_type(context,
                                                             schemas.type[alternative_row],
                                                             schemas.count[alternative_row]),
                                            schemas.name[alternative_row]));
    }
    auto const created_record{
        ir.record(unions.name[row], alternatives.build(), RecordKind::union_)};
    ir.set_record_export_specifier(created_record, unions.export_specifier[row]);
    return created_record;
}

} // namespace

auto lower_union(LoweringContext& context, DeclarationHandle schema) -> DeclarationEmission {
    return {.header = {union_node(context, schema)}};
}

} // namespace lispb::codegen::detail
