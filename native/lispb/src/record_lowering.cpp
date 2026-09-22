#include "lowering.h"
#include "lowering_utils.h"

#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

namespace codegen::detail {
namespace {

auto record_member_type(RecordMemberSchema const& member,
                        std::map<std::string, CppType> const& types) -> CppType {
    auto element{resolve_type(member.type, types)};
    if (!member.count.has_value()) {
        return element;
    }
    TypeDependency array_dependency{"std::array", "array", std::move(element.dependencies)};
    return CppType{"std::array<" + element.spelling + ", " + std::to_string(*member.count) + ">",
                   {std::move(array_dependency)}};
}

auto record_node(RecordSchema const& record, std::map<std::string, CppType> const& types) -> Node {
    NodeListBuilder members;
    for (auto const& member : record.members) {
        members.add(Member{record_member_type(member, types), member.name});
    }
    return Struct{
        .name = record.name,
        .children = members.build(),
        .export_specifier = record.export_specifier,
    };
}

} // namespace

auto lower_record(RecordSchema const& schema, std::map<std::string, CppType> const& types)
    -> DeclarationEmission {
    return {.header = {record_node(schema, types)}};
}

} // namespace codegen::detail
