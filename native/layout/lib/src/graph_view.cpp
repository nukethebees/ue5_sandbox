#include <ioj/layout/graph_view.hpp>

#include <codegen/ast/cpp_types.h>
#include <lispb/schema/enum_types.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/integer_scalar_types.h>
#include <lispb/schema/integer_varint_types.h>
#include <lispb/schema/linear_quantized_types.h>
#include <lispb/schema/optional_presence_bit_types.h>
#include <lispb/schema/optional_sentinel_types.h>
#include <lispb/schema/record_types.h>
#include <lispb/schema/soa_columns.h>
#include <lispb/schema/soa_types.h>
#include <lispb/schema/type_graph.h>
#include <lispb/schema/union_types.h>
#include <lispb/strings.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace ioj::layout {
using namespace lispb::schema;
namespace {
void append_edge_label(std::vector<std::string>& labels, std::string value) {
    if (!value.empty() && std::ranges::find(labels, value) == labels.end()) {
        labels.push_back(std::move(value));
    }
}
auto relationship_label(TypeGraph const& types, lispb::schema::RelationshipId const relationship)
    -> std::string {
    auto const& relationships{types.relationships()};
    auto result{
        std::string{lispb::codegen::to_serialized_string(relationships.kind[relationship])}};
    if (relationships.unit[relationship].has_value()) {
        result += " (";
        result += lispb::codegen::to_serialized_string(*relationships.unit[relationship]);
        result += ')';
    }
    return result;
}

} // namespace

auto graph_edge_labels(TypeGraph const& types, TypeId const user, TypeId const dependency)
    -> std::vector<std::string> {
    auto const& defs{types.definitions()};
    auto const& occurrences{types.occurrences()};
    auto const& relationships{types.relationships()};
    auto const& strings{types.strings()};
    auto const definition{types.definition(user)};
    std::vector<std::string> result;
    switch (definition.kind()) {
        case TypeDefinitionKind::enumeration: {
            auto const underlying{defs.enums.underlying_type[definition]};
            if (is_valid(underlying) && occurrences.type[underlying] == dependency) {
                append_edge_label(result, "underlying");
            }
            break;
        }
        case TypeDefinitionKind::integer_scalar: {
            auto const relationship{defs.integer_scalars.relationship[definition]};
            if (is_valid(relationship) &&
                occurrences.type[relationships.target[relationship]] == dependency) {
                append_edge_label(result, relationship_label(types, relationship));
            }
            break;
        }
        case TypeDefinitionKind::linear_quantized:
            if (occurrences.type[defs.linear_quantized.source[definition]] == dependency) {
                append_edge_label(result, "quantises");
            }
            break;
        case TypeDefinitionKind::integer_varint:
            if (occurrences.type[defs.integer_varints.source[definition]] == dependency) {
                append_edge_label(result, "encodes");
            }
            break;
        case TypeDefinitionKind::optional_sentinel: {
            auto const& optionals{defs.optional_sentinels};
            if (occurrences.type[optionals.source[definition]] == dependency) {
                append_edge_label(result,
                                  "optional via " + std::string{strings.view(
                                                        optionals.sentinel_name[definition])});
            }
            break;
        }
        case TypeDefinitionKind::optional_presence_bit:
            if (occurrences.type[defs.optional_presence_bits.source[definition]] == dependency) {
                append_edge_label(result, "optional via presence bit");
            }
            break;
        case TypeDefinitionKind::soa: {
            auto const& soas{defs.soas};
            auto const& columns{defs.soa_columns};
            auto const equivalent{soas.equivalent_type[definition]};
            if (is_valid(equivalent) && occurrences.type[equivalent] == dependency) {
                append_edge_label(result, "equivalent row");
            }
            for (auto const column : soas.columns_of(definition)) {
                auto const name{std::string{strings.view(columns.name[column])}};
                if (occurrences.type[columns.semantic_type[column]] == dependency) {
                    append_edge_label(
                        result,
                        name + (columns.kind[column] == lispb::codegen::SoaMemberKind::nested
                                    ? " nested"
                                    : ""));
                }
                auto const relationship{columns.relationship[column]};
                if (is_valid(relationship) &&
                    occurrences.type[relationships.target[relationship]] == dependency) {
                    append_edge_label(result, name + " " + relationship_label(types, relationship));
                }
            }
            break;
        }
        case TypeDefinitionKind::record:
        case TypeDefinitionKind::union_type: {
            auto const children{definition.kind() == TypeDefinitionKind::record
                                    ? defs.records.members_of(definition)
                                    : defs.unions.alternatives_of(definition)};
            auto const& table{defs.children};
            for (auto const child : children) {
                if (table.reserved[child]) {
                    continue;
                }
                auto const name{std::string{strings.view(table.name[child])}};
                if (occurrences.type[table.semantic_type[child]] == dependency) {
                    auto label{name};
                    if (table.case_label[child]) {
                        label += " [" + std::string{strings.view(*table.case_label[child])} + "]";
                    }
                    append_edge_label(result, std::move(label));
                }
                auto const relationship{table.relationship[child]};
                if (is_valid(relationship) &&
                    occurrences.type[relationships.target[relationship]] == dependency) {
                    append_edge_label(result, name + " " + relationship_label(types, relationship));
                }
            }
            break;
        }
        default:
            break;
    }
    if (result.empty()) {
        auto const owner{types.source_declaration(user)};
        for (auto const& use : types.type_uses()) {
            if (types.declaration(use) == owner &&
                occurrences.type[types.target(use)] == dependency) {
                append_edge_label(
                    result, lispb::codegen::type_reference_role_text(strings, types.role(use)));
            }
        }
    }
    if (result.empty()) {
        result.push_back("references");
    }
    return result;
}

auto graph_label_summary(std::span<std::string const> labels) -> std::string {
    std::string result;
    auto const count{std::min(labels.size(), std::size_t{2})};
    for (std::size_t index{}; index < count; ++index) {
        if (index != 0) {
            result += ", ";
        }
        result += labels[index];
    }
    if (result.size() > 48) {
        auto end{std::size_t{45}};
        while (end > 0 && (static_cast<unsigned char>(result[end]) & 0xc0U) == 0x80U) {
            --end;
        }
        result.resize(end);
        result += "...";
    }
    if (labels.size() > count) {
        result += " (+" + std::to_string(labels.size() - count) + ")";
    }
    return result;
}

auto project_graph(TypeGraph const& graph, GraphScope const& scope) -> GraphProjection {
    GraphProjection result;
    std::set<TypeId> members;
    auto const count{graph.types().size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const& node{graph.types()[index]};
        if ((!scope.module && scope.members.empty()) ||
            (scope.module && graph.strings().view(graph.module_name(node)) == *scope.module) ||
            std::ranges::find(scope.members, graph.identity(node)) != scope.members.end()) {
            members.insert(TypeId{static_cast<std::uint32_t>(index)});
        }
    }
    auto visible{members};
    for (auto const id : members) {
        for (auto const dependency : graph.dependencies_of(id)) {
            visible.insert(dependency);
        }
    }
    std::map<TypeId, std::size_t> occurrence;
    for (auto const id : visible) {
        occurrence[id] = result.nodes.size();
        result.nodes.push_back({id, !members.contains(id)});
    }
    for (auto const id : members) {
        for (auto const dependency : graph.dependencies_of(id)) {
            auto labels{graph_edge_labels(graph, id, dependency)};
            auto summary{graph_label_summary(labels)};
            result.edges.push_back({occurrence.at(id),
                                    occurrence.at(dependency),
                                    std::move(labels),
                                    std::move(summary)});
        }
    }
    return result;
}

auto layout_graph(GraphProjection const& graph, std::span<GraphPoint const> sizes)
    -> std::vector<GraphPoint> {
    auto const count{graph.nodes.size()};
    std::vector<std::vector<std::size_t>> dependencies(count);
    std::vector<std::vector<std::size_t>> users(count);
    for (auto const& edge : graph.edges) {
        dependencies[edge.user].push_back(edge.dependency);
        users[edge.dependency].push_back(edge.user);
    }
    std::vector<bool> visited(count);
    std::vector<std::size_t> order;
    auto const visit{[&](auto const& self, std::size_t node) -> void {
        if (visited[node]) {
            return;
        }
        visited[node] = true;
        for (auto const dependency : dependencies[node]) {
            self(self, dependency);
        }
        order.push_back(node);
    }};
    for (std::size_t node{}; node < count; ++node) {
        visit(visit, node);
    }
    std::vector<std::size_t> component(count, count);
    std::size_t components{};
    auto const assign{[&](auto const& self, std::size_t node) -> void {
        if (component[node] != count) {
            return;
        }
        component[node] = components;
        for (auto const user : users[node]) {
            self(self, user);
        }
    }};
    for (auto it{order.rbegin()}; it != order.rend(); ++it) {
        if (component[*it] == count) {
            assign(assign, *it);
            ++components;
        }
    }
    std::vector<std::vector<std::size_t>> component_dependencies(components);
    for (auto const& edge : graph.edges) {
        if (component[edge.user] != component[edge.dependency]) {
            component_dependencies[component[edge.user]].push_back(component[edge.dependency]);
        }
    }
    std::vector<std::optional<std::size_t>> ranks(components);
    auto const rank{[&](auto const& self, std::size_t group) -> std::size_t {
        if (ranks[group]) {
            return *ranks[group];
        }
        std::size_t value{};
        for (auto const dependency : component_dependencies[group]) {
            value = std::max(value, self(self, dependency) + 1);
        }
        ranks[group] = value;
        return value;
    }};
    std::map<std::size_t, std::vector<std::size_t>> columns;
    for (std::size_t node{}; node < count; ++node) {
        columns[rank(rank, component[node])].push_back(node);
    }
    auto const rows{
        std::max(std::size_t{4},
                 static_cast<std::size_t>(std::ceil(std::sqrt(static_cast<double>(count)) * 1.5)))};
    std::vector<GraphPoint> positions(count);
    float x{32.0F};
    for (auto& [level, nodes] : columns) {
        std::ranges::stable_sort(
            nodes, [&](auto left, auto right) { return component[left] < component[right]; });
        float width{};
        float height{};
        for (auto const node : nodes) {
            width = std::max(width, sizes[node][0]);
            height = std::max(height, sizes[node][1]);
        }
        for (std::size_t index{}; index < nodes.size(); ++index) {
            positions[nodes[index]] = {x + static_cast<float>(index / rows) * (width + 40.0F),
                                       32.0F + static_cast<float>(index % rows) * (height + 32.0F)};
        }
        x += static_cast<float>((nodes.size() + rows - 1) / rows) * (width + 40.0F) + 60.0F;
    }
    return positions;
}

auto fit_graph(std::span<GraphPoint const> positions,
               std::span<GraphPoint const> sizes,
               GraphPoint viewport,
               float const margin) -> GraphViewport {
    if (positions.empty()) {
        return {};
    }
    auto minimum{positions.front()};
    auto maximum{minimum};
    for (std::size_t index{}; index < positions.size(); ++index) {
        for (std::size_t axis{}; axis < 2; ++axis) {
            minimum[axis] = std::min(minimum[axis], positions[index][axis]);
            maximum[axis] = std::max(maximum[axis], positions[index][axis] + sizes[index][axis]);
        }
    }
    GraphViewport result{};
    result.zoom = 2.0F;
    for (std::size_t axis{}; axis < 2; ++axis) {
        viewport[axis] = std::max(viewport[axis], 1.0F);
        auto const available{viewport[axis] - 2.0F * std::min(margin, viewport[axis] * 0.1F)};
        auto const extent{maximum[axis] - minimum[axis]};
        if (extent > 0.0F) {
            result.zoom = std::min(result.zoom, available / extent);
        }
    }
    for (std::size_t axis{}; axis < 2; ++axis) {
        result.pan[axis] = (viewport[axis] - (maximum[axis] - minimum[axis]) * result.zoom) * 0.5F -
                           minimum[axis] * result.zoom;
    }
    return result;
}

auto graph_segment_hit(GraphPoint point, GraphPoint start, GraphPoint end, float const tolerance)
    -> bool {
    auto const dx{end[0] - start[0]};
    auto const dy{end[1] - start[1]};
    auto const length_squared{dx * dx + dy * dy};
    auto const t{
        length_squared > 0.0F
            ? std::clamp(((point[0] - start[0]) * dx + (point[1] - start[1]) * dy) / length_squared,
                         0.0F,
                         1.0F)
            : 0.0F};
    auto const x{point[0] - start[0] - t * dx};
    auto const y{point[1] - start[1] - t * dy};
    return x * x + y * y <= tolerance * tolerance;
}

} // namespace ioj::layout
