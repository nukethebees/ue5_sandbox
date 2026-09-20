#include "lowering.h"
#include "lowering_utils.h"

#include <utility>

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

auto lower_record_module(RecordModuleSchema const& module,
                         std::map<std::string, CppType> const& types) -> Module {
    NodeListBuilder definitions;
    for (std::size_t index{}; index < module.records.size(); ++index) {
        definitions.add(record_node(module.records[index], types),
                        index + 1 < module.records.size() ? 2 : 1);
    }
    auto definition_nodes{definitions.build()};
    if (module.settings.namespace_name.has_value()) {
        definition_nodes = {
            Namespace{*module.settings.namespace_name, std::move(definition_nodes)}};
    }

    NodeListBuilder header_nodes;
    header_nodes.add(IncludeDependencies{}, 2);
    if (!module.settings.prelude_lines.empty()) {
        header_nodes.add(raw(join_lines(module.settings.prelude_lines)), 2);
    }
    header_nodes.append(std::move(definition_nodes));

    return Module{
        .name = module.settings.name,
        .header = CppFile{.path = module.settings.header,
                          .nodes = header_nodes.build(),
                          .clang_format_off = true,
                          .include_order = module.settings.include_order},
    };
}

} // namespace codegen::detail
