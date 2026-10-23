#include <ioj/layout/external_dependency.hpp>

#include <codegen/schema.h>
#include <codegen/schema/declaration_schema.h>
#include <codegen/schema/declarations.h>
#include <lispb/schema/external_types.h>
#include <lispb/schema/type_graph.h>
#include <lispb/strings.h>

#include <algorithm>
#include <map>
#include <utility>

namespace ioj::layout {
auto external_dependencies(lispb::schema::TypeGraph const& types)
    -> std::vector<ExternalDependency> {
    std::map<std::string, ExternalDependency> grouped;
    auto append_unique = [](auto& values, auto const& value) {
        if (std::ranges::find(values, value) == values.end()) {
            values.push_back(value);
        }
    };
    for (std::size_t index{}; index < types.types().size(); ++index) {
        auto const& node{types.types()[index]};
        auto const external{
            ((types.definition(node)).kind() == lispb::schema::TypeDefinitionKind::external
                 ? std::optional{types.definition(node)}
                 : std::nullopt)};
        if (!external) {
            continue;
        }
        if (std::string{types.strings().view(types.cpp_spelling(node))} == "void" ||
            std::string{types.strings().view(types.cpp_spelling(node))} == "auto" ||
            std::string{types.strings().view(types.cpp_spelling(node))} == "decltype(auto)") {
            continue;
        }
        auto& entry{grouped[lispb::codegen::native_spelling(
            std::string{types.strings().view(types.cpp_spelling(node))})]};
        entry.cpp_spelling = lispb::codegen::native_spelling(
            std::string{types.strings().view(types.cpp_spelling(node))});
        entry.types.push_back(lispb::schema::TypeId{static_cast<std::uint32_t>(index)});
        entry.semantics_known =
            entry.semantics_known ||
            (types.definitions()
                 .externals.integer_semantics[types.definitions().externals.index(*external)]
                 .has_value() ||
             types.definitions()
                 .externals.floating_semantics[types.definitions().externals.index(*external)]
                 .has_value());
        for (auto const& name : types.definitions().externals.registered_names_of(*external)) {
            append_unique(entry.registered_names, std::string{types.strings().view(name)});
        }
    }
    for (std::size_t index{}; index < types.type_uses().size(); ++index) {
        auto const& use{types.type_uses()[index]};
        auto const& node{types.occurrences().type[types.occurrences().index(types.target(use))]};
        if (!((types.definition(node)).kind() == lispb::schema::TypeDefinitionKind::external)) {
            continue;
        }
        auto const found{grouped.find(lispb::codegen::native_spelling(
            std::string{types.strings().view(types.cpp_spelling(node))}))};
        if (found == grouped.end()) {
            continue;
        }
        auto& entry{found->second};
        entry.uses.push_back(use);
        append_unique(
            entry.modules,
            std::string{types.strings().view(
                types.declarations()
                    .module_settings_store.name[types.declarations().module_settings_store.index(
                        types.declarations().module_settings(types.module(use)))])});
        if (types.declaration(use).value != 0) {
            auto const settings{types.declarations().module_settings(types.module(use))};
            append_unique(
                entry.declarations,
                lispb::schema::TypeIdentity{
                    .origin = lispb::schema::TypeOrigin::declaration,
                    .module_name =
                        types.declarations()
                            .module_settings_store
                            .name[types.declarations().module_settings_store.index(settings)],
                    .namespace_name =
                        types.declarations()
                            .module_settings_store
                            .namespace_name[types.declarations().module_settings_store.index(
                                settings)]
                            .value_or(lispb::StringId{}),
                    .name = lispb::codegen::declaration_name(types.declarations(),
                                                             types.declaration(use))});
        }
    }
    std::vector<ExternalDependency> result;
    for (auto& [spelling, entry] : grouped) {
        result.push_back(std::move(entry));
    }
    std::ranges::stable_sort(result, [](auto const& left, auto const& right) {
        return left.declarations.size() > right.declarations.size();
    });
    return result;
}
}
