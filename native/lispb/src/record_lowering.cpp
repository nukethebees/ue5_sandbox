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

void visit_record(std::size_t const index,
                  RecordModuleSchema const& module,
                  std::map<std::string, std::size_t, std::less<>> const& records_by_name,
                  std::vector<std::uint8_t>& states,
                  std::vector<std::size_t>& order) {
    if (states[index] == 2) {
        return;
    }
    if (states[index] == 1) {
        throw std::invalid_argument{"Illegal by-value record cycle involving '" +
                                    module.records[index].name + "'"};
    }

    states[index] = 1;
    for (auto const& member : module.records[index].members) {
        if (auto const dependency{records_by_name.find(member.type.name)};
            dependency != records_by_name.end()) {
            visit_record(dependency->second, module, records_by_name, states, order);
        }
    }
    states[index] = 2;
    order.push_back(index);
}

auto record_emission_order(RecordModuleSchema const& module) -> std::vector<std::size_t> {
    std::map<std::string, std::size_t, std::less<>> records_by_name;
    for (std::size_t index{}; index < module.records.size(); ++index) {
        records_by_name.emplace(module.records[index].name, index);
    }

    std::vector<std::uint8_t> states(module.records.size());
    std::vector<std::size_t> order;
    order.reserve(module.records.size());
    for (std::size_t index{}; index < module.records.size(); ++index) {
        visit_record(index, module, records_by_name, states, order);
    }
    return order;
}

} // namespace

auto lower_record(RecordSchema const& schema, std::map<std::string, CppType> const& types)
    -> DeclarationEmission {
    return {.header = {record_node(schema, types)}};
}

auto lower_record_module(RecordModuleSchema const& module,
                         std::map<std::string, CppType> const& types) -> Module {
    std::vector<DeclarationEmission> emissions;
    auto const order{record_emission_order(module)};
    emissions.reserve(order.size());
    for (auto const index : order) {
        emissions.push_back(lower_record(module.records[index], types));
    }
    return assemble_module(module.settings, emissions).front();
}

} // namespace codegen::detail
