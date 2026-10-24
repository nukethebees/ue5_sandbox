#include "settings_fields.h"

#include <codegen/schema/declarations.h>
#include <lispb/schema/type_graph.h>
#include <lispb/strings.h>

#include <algorithm>
#include <stdexcept>

namespace lispb::codegen::detail {
auto settings_record(schema::TypeGraph const& graph,
                     ModuleHandle const module,
                     TypeRefId const reference) -> std::optional<schema::TypeId> {
    auto const& occurrences{graph.occurrences()};
    auto const row{occurrences.index(graph.resolved(module, reference))};
    auto const type{occurrences.type[row]};
    auto const declaration{graph.source_declaration(type)};
    if (occurrences.names_semantic_type[row] && !occurrences.cv_qualified[row] &&
        occurrences.physical_form[row] == PhysicalTypeForm::value && declaration.value != 0 &&
        declaration.kind() == DeclarationKind::record &&
        !graph.declarations()
             .records.constraint_policy[graph.declarations().records.index(declaration)]) {
        return type;
    }
    return std::nullopt;
}

auto settings_fields(schema::TypeGraph const& graph,
                     ModuleHandle const module,
                     DeclarationHandle const schema) -> std::vector<SettingsField> {
    auto const& declarations{graph.declarations()};
    auto const& records{declarations.records};
    auto const& members{declarations.record_members};
    auto const& schemas{declarations.settings_schemas};
    auto const text{[&](StringId const id) { return std::string{graph.strings().view(id)}; }};
    auto const join{[](std::string const& prefix, std::string const& name) {
        return prefix.empty() ? name : prefix + '.' + name;
    }};
    std::vector<SettingsField> result;
    std::vector<schema::TypeId> ancestors;
    auto const expand{[&](auto const& self,
                          schema::TypeId const type,
                          std::string const& path,
                          std::string const& target) -> void {
        if (std::ranges::find(ancestors, type) != ancestors.end()) {
            throw std::invalid_argument{"Recursive settings record: " + path};
        }
        auto const row{records.index(graph.source_declaration(type))};
        if (!records.generate_settings[row]) {
            throw std::invalid_argument{"Settings section requires :generate-settings true: " +
                                        path};
        }
        ancestors.push_back(type);
        for (auto const id : declarations.items(records.members[row])) {
            auto const member{members.index(id)};
            if (members.settings_exclude[member]) {
                continue;
            }
            auto const name{text(members.name[member])};
            auto external{members.settings_name[member] ? text(*members.settings_name[member])
                                                        : name};
            if (!members.settings_name[member]) {
                std::ranges::replace(external, '_', '-');
            }
            auto const field_path{join(path, external)};
            auto const member_path{join(target, name)};
            auto const nested{settings_record(graph, graph.module(type), members.type[member])};
            if (nested && !members.count[member]) {
                self(self, *nested, field_path, member_path);
            } else {
                result.push_back({field_path, member_path});
            }
        }
        ancestors.pop_back();
    }};

    auto const row{schemas.index(schema)};
    auto const paths{declarations.items(schemas.paths[row])};
    auto const targets{declarations.items(schemas.members[row])};
    auto const sections{declarations.items(schemas.records[row])};
    auto const count{paths.size()};
    for (std::size_t index{}; index < count; ++index) {
        if (!sections.empty() && sections[index].value != 0) {
            auto const type{settings_record(graph, module, sections[index])};
            if (!type) {
                throw std::invalid_argument{"Settings section must name an ordinary record"};
            }
            expand(expand, *type, text(paths[index]), text(targets[index]));
        } else {
            result.push_back({text(paths[index]), text(targets[index])});
        }
    }
    return result;
}
}
