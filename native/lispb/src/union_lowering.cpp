#include "declaration_emission.h"
#include "lowering.h"
#include "lowering_context.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/node_id.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/ast/record_kind.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/tagged_union_alternative_schemas.h>
#include <codegen/schema/tagged_union_schemas.h>
#include <codegen/schema/union_alternative_schemas.h>
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
    NodeListBuilder alternatives{ir};
    for (auto const id : context.declarations.items(
             context.declarations.unions.alternatives[context.declarations.unions.index(schema)])) {
        auto const alternative{id};
        alternatives.add(ir.instance_member(
            alternative_type(
                context,
                context.declarations.union_alternatives
                    .type[context.declarations.union_alternatives.index(alternative)],
                context.declarations.union_alternatives
                    .count[context.declarations.union_alternatives.index(alternative)]),
            context.declarations.union_alternatives
                .name[context.declarations.union_alternatives.index(alternative)]));
    }
    auto const created_record{
        ir.record(context.declarations.unions.name[context.declarations.unions.index(schema)],
                  alternatives.build(),
                  RecordKind::union_)};
    ir.set_record_export_specifier(
        created_record,
        context.declarations.unions.export_specifier[context.declarations.unions.index(schema)]);
    return created_record;
}

auto tagged_union_node(LoweringContext& context, DeclarationHandle schema) -> NodeId {
    auto& ir{context.ir};
    NodeListBuilder alternatives{ir};
    for (auto const id : context.declarations.items(
             context.declarations.tagged_unions
                 .alternatives[context.declarations.tagged_unions.index(schema)])) {
        auto const alternative{id};
        alternatives.add(ir.line_comment(
            "tag: " +
            context.text(
                context.declarations.tagged_union_alternatives
                    .tag[context.declarations.tagged_union_alternatives.index(alternative)])));
        alternatives.add(ir.instance_member(
            alternative_type(
                context,
                context.declarations.tagged_union_alternatives
                    .type[context.declarations.tagged_union_alternatives.index(alternative)],
                context.declarations.tagged_union_alternatives
                    .count[context.declarations.tagged_union_alternatives.index(alternative)]),
            context.declarations.tagged_union_alternatives
                .name[context.declarations.tagged_union_alternatives.index(alternative)]));
    }

    NodeListBuilder members{ir};
    members.add(ir.instance_member(
        context.resolve(context.declarations.tagged_unions
                            .discriminant[context.declarations.tagged_unions.index(schema)]),
        ir.strings.intern("tag")));
    members.add(ir.record(ir.strings.intern("Payload"), alternatives.build(), RecordKind::union_));
    members.add(ir.instance_member(ir.types.intern("Payload"), ir.strings.intern("payload")));
    auto const created_record{ir.record(
        context.declarations.tagged_unions.name[context.declarations.tagged_unions.index(schema)],
        members.build(),
        RecordKind::struct_)};
    ir.set_record_export_specifier(
        created_record,
        context.declarations.tagged_unions
            .export_specifier[context.declarations.tagged_unions.index(schema)]);
    return created_record;
}

} // namespace

auto lower_union(LoweringContext& context, DeclarationHandle schema) -> DeclarationEmission {
    return {.header = {union_node(context, schema)}};
}

auto lower_tagged_union(LoweringContext& context, DeclarationHandle schema) -> DeclarationEmission {
    return {.header = {tagged_union_node(context, schema)}};
}

} // namespace lispb::codegen::detail
