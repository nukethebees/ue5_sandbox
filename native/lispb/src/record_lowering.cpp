#include "declaration_emission.h"
#include "lowering.h"
#include "lowering_context.h"
#include "lowering_utils.h"
#include "record_reader_lowering.h"

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
#include <lispb/schema/type_graph.h>
#include <lispb/strings.h>

#include <optional>
#include <string>
#include <vector>

namespace lispb::codegen::detail {
namespace {

auto record_member_type(LoweringContext& context, RecordMemberId member) -> CppTypeId {
    auto& ir{context.ir};
    auto const& members{context.declarations.record_members};
    auto const row{members.index(member)};
    auto const element{context.resolve(members.type[row])};
    auto const count{members.count[row]};
    if (!count) {
        return element;
    }
    return ir.types.template_type(
        ir.types.named("std::array", {ir.types.dependency("std::array", "array")}),
        {element, ir.types.intern(std::to_string(*count))});
}

auto record_node(LoweringContext& context, DeclarationHandle record) -> NodeId {
    auto& ir{context.ir};
    auto const& records{context.declarations.records};
    auto const& record_members{context.declarations.record_members};
    auto const row{records.index(record)};
    auto const name{context.text(records.name[row]) +
                    (records.constraint_policy[row] ? "Value" : "")};
    NodeListBuilder members{ir};
    if (records.comparison[row] != RecordComparison::none) {
        auto const three_way{records.comparison[row] == RecordComparison::three_way};
        auto const comparison{ir.function(
            three_way ? "operator<=>" : "operator==",
            three_way ? ir.types.intern("auto", {ir.types.dependency("auto", "compare")})
                      : ir.types.intern("bool"))};
        ir.set_function_parameters(comparison, {ir.parameter(name + " const&", "")});
        ir.set_function_receiver(comparison, FunctionReceiver::const_);
        if (records.comparison_noexcept[row]) {
            ir.set_function_noexcept(comparison);
        }
        ir.set_function_defaulted(comparison);
        members.add(ir.header_function(comparison));
    }
    for (auto const function : context.declarations.items(records.functions[row])) {
        members.add(ir.header_function(lower_schema_function(context, function)));
    }
    if (records.comparison[row] != RecordComparison::none || !records.functions[row].empty()) {
        members.new_lines(2);
    }
    for (auto const member : context.declarations.items(records.members[row])) {
        auto const member_row{record_members.index(member)};
        auto const initializer{record_members.initializer[member_row]};
        members.add(ir.instance_member(record_member_type(context, member),
                                       record_members.name[member_row],
                                       initializer
                                           ? std::optional<Expr>{ir.raw_expr(*initializer, {})}
                                           : std::nullopt),
                    1);
    }
    auto const created_record{
        ir.record(ir.strings.intern(name), members.build(), RecordKind::struct_)};
    ir.set_record_export_specifier(created_record, records.export_specifier[row]);
    return created_record;
}

} // namespace

auto lower_record(LoweringContext& context, DeclarationHandle schema) -> DeclarationEmission {
    auto result{DeclarationEmission{.header = {record_node(context, schema)}}};
    auto const& records{context.declarations.records};
    auto const row{records.index(schema)};
    if (auto const policy{records.constraint_policy[row]}) {
        auto const name{context.text(records.name[row])};
        auto& ir{context.ir};
        auto const constrained{ir.types.intern(
            "ioj::ConstrainedValue<" + name + "Value, " + context.text(*policy) + ">",
            {ir.types.dependency("ioj::ConstrainedValue", "ioj/constrained_value.h")})};
        result.header.push_back(ir.using_declaration(records.name[row], constrained));
    }
    if (records.s7_tag[records.index(schema)]) {
        lower_record_reader(context, schema, result);
    }
    return result;
}

} // namespace lispb::codegen::detail
