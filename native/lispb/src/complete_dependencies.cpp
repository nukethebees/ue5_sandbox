#include <codegen/complete_dependencies.h>

#include <codegen/schema/type_reference_kind.h>
#include <codegen/type_dependency_edge.h>
#include <lispb/schema/record_types.h>
#include <lispb/schema/soa_columns.h>
#include <lispb/schema/soa_types.h>
#include <lispb/schema/type_definition_kind.h>
#include <lispb/schema/type_graph.h>
#include <lispb/schema/union_types.h>
#include <lispb/strings.h>

#include <algorithm>
#include <cstddef>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>

namespace lispb::codegen::detail {

auto CompleteDependencies::of(schema::TypeId type) const -> std::span<schema::TypeId const> {
    return std::span{targets}.subspan(offsets[type.value],
                                      offsets[type.value + 1] - offsets[type.value]);
}

auto is_record_signature(schema::TypeUseId const use, schema::TypeGraph const& graph) -> bool {
    return graph.kind(use) != TypeReferenceKind::ordinary && graph.owner(use) &&
           graph.definition(*graph.owner(use)).kind() == schema::TypeDefinitionKind::record;
}

auto complete_dependencies(schema::TypeGraph const& graph) -> CompleteDependencies {
    using namespace lispb::schema;
    std::vector<TypeDependencyEdge> edges;
    auto const& definitions{graph.definitions()};
    auto const count{static_cast<std::uint32_t>(graph.types().size())};
    for (std::uint32_t index{}; index < count; ++index) {
        auto const owner{TypeId{index}};
        auto dependency = [&](ResolvedOccurrenceId reference) {
            if (is_valid(reference) && !graph.strings().view(graph.cpp_spelling(owner)).empty() &&
                !graph.strings()
                     .view(graph.cpp_spelling(
                         graph.occurrences().type[graph.occurrences().index(reference)]))
                     .empty() &&
                !uses_forward_declaration(reference, graph)) {
                edges.push_back(
                    {owner, graph.occurrences().type[graph.occurrences().index(reference)]});
            }
        };
        if (is_packed_projection(graph, owner)) {
            for (auto const& placement : graph.bit_layout(owner).placements) {
                if (placement.reserved) {
                    continue;
                }
                auto const kind{graph.definition(placement.type).kind()};
                if (kind == TypeDefinitionKind::enumeration ||
                    ((kind == TypeDefinitionKind::record ||
                      kind == TypeDefinitionKind::union_type) &&
                     is_packed_projection(graph, placement.type))) {
                    dependency(definitions.children
                                   .semantic_type[definitions.children.index(placement.child)]);
                }
            }
            continue;
        }
        switch (graph.definition(owner).kind()) {
            case TypeDefinitionKind::record: {
                for (auto const id : definitions.records.members_of(graph.definition(owner))) {
                    dependency(definitions.children.semantic_type[definitions.children.index(id)]);
                }
                break;
            }
            case TypeDefinitionKind::soa: {
                for (auto const id : definitions.soas.columns_of(graph.definition(owner))) {
                    dependency(
                        definitions.soa_columns.semantic_type[definitions.soa_columns.index(id)]);
                }
                break;
            }
            case TypeDefinitionKind::union_type: {
                for (auto const id : definitions.unions.alternatives_of(graph.definition(owner))) {
                    dependency(definitions.children.semantic_type[definitions.children.index(id)]);
                }
                break;
            }

            default:
                break;
        }
    }
    for (auto const use : graph.type_uses()) {
        if (is_record_signature(use, graph) &&
            graph.occurrences().type[graph.occurrences().index(graph.target(use))] !=
                *graph.owner(use) &&
            !uses_forward_declaration(graph.target(use), graph, graph.kind(use))) {
            edges.push_back(
                {*graph.owner(use),
                 graph.occurrences().type[graph.occurrences().index(graph.target(use))]});
        }
    }
    std::ranges::stable_sort(edges, {}, &TypeDependencyEdge::owner);
    CompleteDependencies result;
    result.offsets.reserve(count + 1);
    result.targets.reserve(edges.size());
    std::size_t position{};
    for (std::uint32_t index{}; index < count; ++index) {
        result.offsets.push_back(static_cast<std::uint32_t>(result.targets.size()));
        while (position < edges.size() && edges[position].owner.value == index) {
            result.targets.push_back(edges[position++].target);
        }
    }
    result.offsets.push_back(static_cast<std::uint32_t>(result.targets.size()));
    return result;
}

void validate_complete_definition_cycles(schema::TypeGraph const& graph,
                                         CompleteDependencies const& dependencies) {
    std::vector<bool> visited(graph.types().size());
    std::vector<schema::TypeId> active;
    auto visit = [&](auto&& self, schema::TypeId type) -> void {
        if (visited[type.value]) {
            return;
        }
        if (auto const cycle{std::ranges::find(active, type)}; cycle != active.end()) {
            std::string message{"C++ complete-definition dependency cycle: "};
            for (auto entry{cycle}; entry != active.end(); ++entry) {
                message += graph.strings().view(graph.cpp_spelling(*entry));
                message += " -> ";
            }
            message += graph.strings().view(graph.cpp_spelling(type));
            throw std::invalid_argument{message};
        }
        active.push_back(type);
        for (auto const dependency : dependencies.of(type)) {
            self(self, dependency);
        }
        active.pop_back();
        visited[type.value] = true;
    };
    auto const count{static_cast<std::uint32_t>(graph.types().size())};
    for (std::uint32_t index{}; index < count; ++index) {
        visit(visit, schema::TypeId{index});
    }
}

}
