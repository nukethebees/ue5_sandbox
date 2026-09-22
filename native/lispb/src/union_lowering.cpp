#include "lowering.h"
#include "lowering_utils.h"

#include <map>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace codegen::detail {
namespace {

auto alternative_type(UnionAlternativeSchema const& alternative,
                      std::map<std::string, CppType> const& types) -> CppType {
    auto element{resolve_type(alternative.type, types)};
    if (!alternative.count.has_value()) {
        return element;
    }
    TypeDependency array_dependency{"std::array", "array", std::move(element.dependencies)};
    return CppType{"std::array<" + element.spelling + ", " + std::to_string(*alternative.count) +
                       ">",
                   {std::move(array_dependency)}};
}

auto union_node(UnionSchema const& schema, std::map<std::string, CppType> const& types) -> Node {
    NodeListBuilder alternatives;
    for (auto const& alternative : schema.alternatives) {
        alternatives.add(Member{alternative_type(alternative, types), alternative.name});
    }
    return Struct{.name = schema.name,
                  .children = alternatives.build(),
                  .export_specifier = schema.export_specifier,
                  .record_kind = "union"};
}

auto tagged_union_node(TaggedUnionSchema const& schema, std::map<std::string, CppType> const& types)
    -> Node {
    NodeListBuilder alternatives;
    for (auto const& alternative : schema.alternatives) {
        alternatives.add(LineComment{"tag: " + alternative.tag});
        alternatives.add(Member{alternative_type(UnionAlternativeSchema{.name = alternative.name,
                                                                        .type = alternative.type,
                                                                        .count = alternative.count},
                                                 types),
                                alternative.name});
    }

    NodeListBuilder members;
    members.add(Member{resolve_type(schema.discriminant, types), "tag"});
    members.add(
        Struct{.name = "Payload", .children = alternatives.build(), .record_kind = "union"});
    members.add(Member{CppType{"Payload"}, "payload"});
    return Struct{.name = schema.name,
                  .children = members.build(),
                  .export_specifier = schema.export_specifier};
}

struct UnionDeclaration {
    bool tagged{};
    std::size_t index{};
};

auto declaration_name(UnionModuleSchema const& module, UnionDeclaration const declaration)
    -> std::string const& {
    return declaration.tagged ? module.tagged_unions[declaration.index].name
                              : module.unions[declaration.index].name;
}

template <typename Function>
void for_each_reference(UnionModuleSchema const& module,
                        UnionDeclaration const declaration,
                        Function&& function) {
    if (declaration.tagged) {
        auto const& schema{module.tagged_unions[declaration.index]};
        function(schema.discriminant);
        for (auto const& alternative : schema.alternatives) {
            function(alternative.type);
        }
    } else {
        for (auto const& alternative : module.unions[declaration.index].alternatives) {
            function(alternative.type);
        }
    }
}

void visit_union(std::size_t const declaration_index,
                 UnionModuleSchema const& module,
                 std::span<UnionDeclaration const> const declarations,
                 std::map<std::string, std::size_t, std::less<>> const& unions_by_name,
                 std::vector<std::uint8_t>& states,
                 std::vector<std::size_t>& order) {
    if (states[declaration_index] == 2) {
        return;
    }
    if (states[declaration_index] == 1) {
        throw std::invalid_argument{"Illegal by-value union cycle involving '" +
                                    declaration_name(module, declarations[declaration_index]) +
                                    "'"};
    }

    states[declaration_index] = 1;
    for_each_reference(module, declarations[declaration_index], [&](TypeRef const& reference) {
        if (auto const dependency{unions_by_name.find(reference.name)};
            dependency != unions_by_name.end()) {
            visit_union(dependency->second, module, declarations, unions_by_name, states, order);
        }
    });
    states[declaration_index] = 2;
    order.push_back(declaration_index);
}

auto union_declarations(UnionModuleSchema const& module) -> std::vector<UnionDeclaration> {
    std::vector<UnionDeclaration> result;
    result.reserve(module.unions.size() + module.tagged_unions.size());
    for (std::size_t index{}; index < module.unions.size(); ++index) {
        result.push_back({.tagged = false, .index = index});
    }
    for (std::size_t index{}; index < module.tagged_unions.size(); ++index) {
        result.push_back({.tagged = true, .index = index});
    }
    return result;
}

auto union_emission_order(UnionModuleSchema const& module,
                          std::span<UnionDeclaration const> const declarations)
    -> std::vector<std::size_t> {
    std::map<std::string, std::size_t, std::less<>> unions_by_name;
    for (std::size_t index{}; index < declarations.size(); ++index) {
        unions_by_name.emplace(declaration_name(module, declarations[index]), index);
    }

    std::vector<std::uint8_t> states(declarations.size());
    std::vector<std::size_t> order;
    order.reserve(declarations.size());
    for (std::size_t index{}; index < declarations.size(); ++index) {
        visit_union(index, module, declarations, unions_by_name, states, order);
    }
    return order;
}

} // namespace

auto lower_union(UnionSchema const& schema, std::map<std::string, CppType> const& types)
    -> DeclarationEmission {
    return {.header = {union_node(schema, types)}};
}

auto lower_tagged_union(TaggedUnionSchema const& schema,
                        std::map<std::string, CppType> const& types) -> DeclarationEmission {
    return {.header = {tagged_union_node(schema, types)}};
}

auto lower_union_module(UnionModuleSchema const& module,
                        std::map<std::string, CppType> const& types) -> Module {
    std::vector<DeclarationEmission> emissions;
    auto const declarations{union_declarations(module)};
    auto const order{union_emission_order(module, declarations)};
    emissions.reserve(order.size());
    for (auto const position : order) {
        auto const declaration{declarations[position]};
        emissions.push_back(declaration.tagged
                                ? lower_tagged_union(module.tagged_unions[declaration.index], types)
                                : lower_union(module.unions[declaration.index], types));
    }
    return assemble_module(module.settings, emissions).front();
}

} // namespace codegen::detail
