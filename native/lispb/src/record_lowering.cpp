#include "lowering.h"
#include "lowering_utils.h"

#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

namespace codegen::detail {
namespace {

auto record_member_type(RecordMemberSchema const& member, TypeRegistry const& types) -> CppType {
    auto element{resolve_type(member.type, types)};
    if (!member.count.has_value()) {
        return element;
    }
    TypeDependency array_dependency{"std::array", "array", std::move(element.dependencies)};
    return CppType{"std::array<" + element.spelling + ", " + std::to_string(*member.count) + ">",
                   {std::move(array_dependency)}};
}

auto record_node(RecordSchema const& record, TypeRegistry const& types) -> Node {
    NodeListBuilder members;
    for (auto const& member : record.members) {
        members.add(Member{record_member_type(member, types),
                           member.name,
                           member.initializer
                               ? std::optional<Expr>{RawExpr{*member.initializer, {}}}
                               : std::nullopt,
                           {}});
    }
    if (record.comparison != RecordComparison::none) {
        auto const three_way{record.comparison == RecordComparison::three_way};
        members.add(Function{
            .spec = {.name = three_way ? "operator<=>" : "operator==",
                     .return_type = three_way ? CppType{"auto", "compare"} : CppType{"bool"},
                     .parameters = {{CppType{record.name + " const&"}, ""}},
                     .qualifiers = {.is_const = true,
                                    .is_noexcept = record.comparison_noexcept,
                                    .disposition = FunctionDisposition::defaulted}},
            .is_header = true});
    }
    for (auto const& function : record.functions) {
        members.add(Function{.spec = schema_function_spec(function, types),
                             .declaration = function.body_lines.empty(),
                             .is_header = true});
    }
    return Struct{
        .name = record.name,
        .children = members.build(),
        .export_specifier = record.export_specifier,
    };
}

} // namespace

auto lower_record(RecordSchema const& schema, TypeRegistry const& types) -> DeclarationEmission {
    return {.header = {record_node(schema, types)}};
}

} // namespace codegen::detail
