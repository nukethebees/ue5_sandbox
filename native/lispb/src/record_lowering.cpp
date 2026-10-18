#include "declaration_emission.h"
#include "lowering.h"
#include "lowering_context.h"
#include "lowering_utils.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/expr.h>
#include <codegen/ast/function_receiver.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/node_id.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/function_schemas.h>
#include <codegen/schema/record_comparison.h>
#include <codegen/schema/record_member_schemas.h>
#include <codegen/schema/record_schemas.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <optional>
#include <string>
#include <vector>

namespace lispb::codegen::detail {
namespace {

auto record_member_type(LoweringContext& context, RecordMemberId member) -> CppTypeId {
    auto& ir{context.ir};
    auto element{context.resolve(context.declarations.record_members
                                     .type[context.declarations.record_members.index(member)])};
    if (!context.declarations.record_members
             .count[context.declarations.record_members.index(member)]
             .has_value()) {
        return element;
    }
    auto const nested{ir.types.dependencies(element)};
    std::vector<TypeDependencyId> dependencies{nested.begin(), nested.end()};
    dependencies.push_back(ir.types.dependency("std::array", "array"));
    return ir.types.intern(
        "std::array<" + std::string{ir.types.spelling(element)} + ", " +
            std::to_string(*context.declarations.record_members
                                .count[context.declarations.record_members.index(member)]) +
            ">",
        dependencies);
}

auto record_node(LoweringContext& context, DeclarationHandle record) -> NodeId {
    auto& ir{context.ir};
    NodeListBuilder members{ir};
    for (auto const id : context.declarations.items(
             context.declarations.records.members[context.declarations.records.index(record)])) {
        auto const member{id};
        members.add(ir.instance_member(
            record_member_type(context, member),
            context.declarations.record_members
                .name[context.declarations.record_members.index(member)],
            context.declarations.record_members
                    .initializer[context.declarations.record_members.index(member)]
                ? std::optional<Expr>{ir.raw_expression(
                      *context.declarations.record_members
                           .initializer[context.declarations.record_members.index(member)],
                      {})}
                : std::nullopt));
    }
    if (context.declarations.records.comparison[context.declarations.records.index(record)] !=
        RecordComparison::none) {
        auto const three_way{
            context.declarations.records.comparison[context.declarations.records.index(record)] ==
            RecordComparison::three_way};
        members.add(ir.header_function([&] {
            auto const created_function{ir.function(
                ir.strings.intern(three_way ? "operator<=>" : "operator=="),
                three_way ? ir.types.intern("auto", {ir.types.dependency("auto", "compare")})
                          : ir.types.intern("bool"))};
            ir.set_function_parameters(
                created_function,
                {ir.parameter(
                    ir.types.intern(
                        context.text(context.declarations.records
                                         .name[context.declarations.records.index(record)]) +
                        " const&"),
                    ir.strings.intern(""))});
            ir.set_function_receiver(created_function, FunctionReceiver::const_);
            if (context.declarations.records
                    .comparison_noexcept[context.declarations.records.index(record)]) {
                ir.set_function_noexcept(created_function);
            }
            ir.set_function_defaulted(created_function);
            return created_function;
        }()));
    }
    for (auto const id : context.declarations.items(
             context.declarations.records.functions[context.declarations.records.index(record)])) {
        auto const function{id};
        members.add(ir.header_function(lower_schema_function(context, function)));
    }
    auto const created_record{
        ir.record(context.declarations.records.name[context.declarations.records.index(record)],
                  members.build(),
                  RecordKind::struct_)};
    ir.set_record_export_specifier(
        created_record,
        context.declarations.records.export_specifier[context.declarations.records.index(record)]);
    return created_record;
}

} // namespace

auto lower_record(LoweringContext& context, DeclarationHandle schema) -> DeclarationEmission {
    return {.header = {record_node(context, schema)}};
}

} // namespace lispb::codegen::detail
