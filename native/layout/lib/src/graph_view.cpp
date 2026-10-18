#include <ioj/layout/graph_view.hpp>

#include <codegen/ast/cpp_types.h>
#include <lispb/schema/enum_types.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/integer_scalar_types.h>
#include <lispb/schema/integer_varint_types.h>
#include <lispb/schema/linear_quantized_types.h>
#include <lispb/schema/optional_presence_bit_types.h>
#include <lispb/schema/optional_sentinel_types.h>
#include <lispb/schema/packed_fields.h>
#include <lispb/schema/packed_types.h>
#include <lispb/schema/record_members.h>
#include <lispb/schema/record_types.h>
#include <lispb/schema/soa_columns.h>
#include <lispb/schema/soa_types.h>
#include <lispb/schema/tagged_union_alternatives.h>
#include <lispb/schema/tagged_union_types.h>
#include <lispb/schema/type_graph.h>
#include <lispb/schema/union_alternatives.h>
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
    auto result{std::string{lispb::codegen::semantic_relation_kind_name(
        types.relationships().kind[types.relationships().index(relationship)])}};
    if (types.relationships().unit[types.relationships().index(relationship)].has_value()) {
        result += " (";
        result += lispb::codegen::semantic_relation_unit_name(
            *types.relationships().unit[types.relationships().index(relationship)]);
        result += ')';
    }
    return result;
}

} // namespace

auto graph_edge_labels(TypeGraph const& types, TypeId const user, TypeId const dependency)
    -> std::vector<std::string> {
    auto const& definition{types.definition(user)};
    std::vector<std::string> result;
    if (auto const enumeration{
            ((definition).kind() == lispb::schema::TypeDefinitionKind::enumeration
                 ? std::optional{definition}
                 : std::nullopt)}) {
        if (lispb::schema::is_valid(
                types.definitions()
                    .enums.underlying_type[types.definitions().enums.index(*enumeration)]) &&
            types.occurrences().type[types.occurrences().index(
                types.definitions()
                    .enums.underlying_type[types.definitions().enums.index(*enumeration)])] ==
                dependency) {
            append_edge_label(result, "underlying");
        }
    } else if (auto const scalar{
                   ((definition).kind() == lispb::schema::TypeDefinitionKind::integer_scalar
                        ? std::optional{definition}
                        : std::nullopt)}) {
        if (lispb::schema::is_valid(
                types.definitions()
                    .integer_scalars
                    .relationship[types.definitions().integer_scalars.index(*scalar)]) &&
            types.occurrences().type[types.occurrences().index(
                types.relationships().target[types.relationships().index(
                    types.definitions().integer_scalars.relationship
                        [types.definitions().integer_scalars.index(*scalar)])])] == dependency) {
            append_edge_label(
                result,
                relationship_label(
                    types,
                    types.definitions()
                        .integer_scalars
                        .relationship[types.definitions().integer_scalars.index(*scalar)]));
        }
    } else if (auto const quantized{
                   ((definition).kind() == lispb::schema::TypeDefinitionKind::linear_quantized
                        ? std::optional{definition}
                        : std::nullopt)}) {
        if (types.occurrences().type[types.occurrences().index(
                types.definitions().linear_quantized.source
                    [types.definitions().linear_quantized.index(*quantized)])] == dependency) {
            append_edge_label(result, "quantises");
        }
    } else if (auto const varint{
                   ((definition).kind() == lispb::schema::TypeDefinitionKind::integer_varint
                        ? std::optional{definition}
                        : std::nullopt)}) {
        if (types.occurrences().type[types.occurrences().index(
                types.definitions()
                    .integer_varints.source[types.definitions().integer_varints.index(*varint)])] ==
            dependency) {
            append_edge_label(result, "encodes");
        }
    } else if (auto const optional{
                   ((definition).kind() == lispb::schema::TypeDefinitionKind::optional_sentinel
                        ? std::optional{definition}
                        : std::nullopt)}) {
        if (types.occurrences().type[types.occurrences().index(
                types.definitions().optional_sentinels.source
                    [types.definitions().optional_sentinels.index(*optional)])] == dependency) {
            append_edge_label(
                result,
                "optional via " +
                    std::string{types.cpp_types().strings().view(
                        types.definitions().optional_sentinels.sentinel_name
                            [types.definitions().optional_sentinels.index(*optional)])});
        }
    } else if (auto const optional{
                   ((definition).kind() == lispb::schema::TypeDefinitionKind::optional_presence_bit
                        ? std::optional{definition}
                        : std::nullopt)}) {
        if (types.occurrences().type[types.occurrences().index(
                types.definitions().optional_presence_bits.source
                    [types.definitions().optional_presence_bits.index(*optional)])] == dependency) {
            append_edge_label(result, "optional via presence bit");
        }
    } else if (auto const packed{((definition).kind() == lispb::schema::TypeDefinitionKind::packed
                                      ? std::optional{definition}
                                      : std::nullopt)}) {
        if (types.occurrences().type[types.occurrences().index(
                types.definitions()
                    .packed.storage_type[types.definitions().packed.index(*packed)])] ==
            dependency) {
            append_edge_label(result, "storage");
        }
        for (auto const& segment : types.definitions().packed.segments_of(*packed)) {
            if (auto const field{((segment).kind() == lispb::schema::PackedSegmentKind::field
                                      ? std::optional{segment}
                                      : std::nullopt)};
                field) {
                if (types.occurrences().type[types.occurrences().index(
                        types.definitions().packed_fields.semantic_type
                            [types.definitions().packed_fields.index(*field)])] == dependency) {
                    append_edge_label(
                        result,
                        std::string{types.strings().view(
                            types.definitions()
                                .packed_fields
                                .name[types.definitions().packed_fields.index(*field)])});
                }
                if (lispb::schema::is_valid(
                        types.definitions()
                            .packed_fields
                            .relationship[types.definitions().packed_fields.index(*field)]) &&
                    types.occurrences().type[types.occurrences().index(
                        types.relationships().target[types.relationships().index(
                            types.definitions().packed_fields.relationship
                                [types.definitions().packed_fields.index(*field)])])] ==
                        dependency) {
                    append_edge_label(
                        result,
                        std::string{types.strings().view(
                            types.definitions()
                                .packed_fields
                                .name[types.definitions().packed_fields.index(*field)])} +
                            " " +
                            relationship_label(
                                types,
                                types.definitions().packed_fields.relationship
                                    [types.definitions().packed_fields.index(*field)]));
                }
            }
        }
    } else if (auto const soa{((definition).kind() == lispb::schema::TypeDefinitionKind::soa
                                   ? std::optional{definition}
                                   : std::nullopt)}) {
        if (lispb::schema::is_valid(
                types.definitions().soas.equivalent_type[types.definitions().soas.index(*soa)]) &&
            types.occurrences().type[types.occurrences().index(
                types.definitions().soas.equivalent_type[types.definitions().soas.index(*soa)])] ==
                dependency) {
            append_edge_label(result, "equivalent row");
        }
        for (auto const column_id : types.definitions().soas.columns_of(*soa)) {
            auto const column{column_id};
            if (types.occurrences().type[types.occurrences().index(
                    types.definitions().soa_columns.semantic_type
                        [types.definitions().soa_columns.index(column)])] == dependency) {
                append_edge_label(
                    result,
                    std::string{types.strings().view(
                        types.definitions()
                            .soa_columns.name[types.definitions().soa_columns.index(column)])});
            }
            if (types.definitions()
                    .soa_columns.nested_type[types.definitions().soa_columns.index(column)] ==
                dependency) {
                append_edge_label(
                    result,
                    std::string{types.strings().view(
                        types.definitions()
                            .soa_columns.name[types.definitions().soa_columns.index(column)])} +
                        " nested");
            }
            if (lispb::schema::is_valid(
                    types.definitions()
                        .soa_columns.relationship[types.definitions().soa_columns.index(column)]) &&
                types.occurrences().type[types.occurrences().index(
                    types.relationships().target[types.relationships().index(
                        types.definitions().soa_columns.relationship
                            [types.definitions().soa_columns.index(column)])])] == dependency) {
                append_edge_label(
                    result,
                    std::string{types.strings().view(
                        types.definitions()
                            .soa_columns.name[types.definitions().soa_columns.index(column)])} +
                        " " +
                        relationship_label(
                            types,
                            types.definitions()
                                .soa_columns
                                .relationship[types.definitions().soa_columns.index(column)]));
            }
        }
    } else if (auto const record{((definition).kind() == lispb::schema::TypeDefinitionKind::record
                                      ? std::optional{definition}
                                      : std::nullopt)}) {
        for (auto const member_id : types.definitions().records.members_of(*record)) {
            auto const member{member_id};
            if (types.occurrences().type[types.occurrences().index(
                    types.definitions().record_members.semantic_type
                        [types.definitions().record_members.index(member)])] == dependency) {
                append_edge_label(
                    result,
                    std::string{types.strings().view(
                        types.definitions()
                            .record_members
                            .name[types.definitions().record_members.index(member)])});
            }
            if (lispb::schema::is_valid(
                    types.definitions()
                        .record_members
                        .relationship[types.definitions().record_members.index(member)]) &&
                types.occurrences().type[types.occurrences().index(
                    types.relationships().target[types.relationships().index(
                        types.definitions().record_members.relationship
                            [types.definitions().record_members.index(member)])])] == dependency) {
                append_edge_label(
                    result,
                    std::string{types.strings().view(
                        types.definitions()
                            .record_members
                            .name[types.definitions().record_members.index(member)])} +
                        " " +
                        relationship_label(
                            types,
                            types.definitions()
                                .record_members
                                .relationship[types.definitions().record_members.index(member)]));
            }
        }
    } else if (auto const union_type{
                   ((definition).kind() == lispb::schema::TypeDefinitionKind::union_type
                        ? std::optional{definition}
                        : std::nullopt)}) {
        for (auto const alternative_id : types.definitions().unions.alternatives_of(*union_type)) {
            auto const alternative{alternative_id};
            if (types.occurrences().type[types.occurrences().index(
                    types.definitions().union_alternatives.semantic_type
                        [types.definitions().union_alternatives.index(alternative)])] ==
                dependency) {
                append_edge_label(
                    result,
                    std::string{types.strings().view(
                        types.definitions()
                            .union_alternatives
                            .name[types.definitions().union_alternatives.index(alternative)])});
            }
        }
    } else if (auto const tagged{
                   ((definition).kind() == lispb::schema::TypeDefinitionKind::tagged_union
                        ? std::optional{definition}
                        : std::nullopt)}) {
        if (types.occurrences().type[types.occurrences().index(
                types.definitions().tagged_unions.discriminant
                    [types.definitions().tagged_unions.index(*tagged)])] == dependency) {
            append_edge_label(result, "discriminates");
        }
        for (auto const alternative_id :
             types.definitions().tagged_unions.alternatives_of(*tagged)) {
            auto const alternative{alternative_id};
            if (types.occurrences().type[types.occurrences().index(
                    types.definitions().tagged_union_alternatives.semantic_type
                        [types.definitions().tagged_union_alternatives.index(alternative)])] ==
                dependency) {
                append_edge_label(
                    result,
                    std::string{types.strings().view(
                        types.definitions().tagged_union_alternatives.name
                            [types.definitions().tagged_union_alternatives.index(alternative)])} +
                        " [" +
                        std::string{types.strings().view(
                            types.definitions()
                                .tagged_union_alternatives
                                .tag[types.definitions().tagged_union_alternatives.index(
                                    alternative)])} +
                        "]");
            }
        }
    }
    if (result.empty()) {
        auto const& node{user};
        auto const owner{types.source_declaration(node)};
        for (auto const& use : types.type_uses()) {
            if (types.declaration(use) == owner &&
                types.occurrences().type[types.occurrences().index(types.target(use))] ==
                    dependency) {
                append_edge_label(result,
                                  lispb::codegen::type_reference_role_text(
                                      types.cpp_types().strings(), types.role(use)));
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
    GraphViewport result{2.0F, {}};
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
