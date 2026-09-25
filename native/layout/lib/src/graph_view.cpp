#include <algorithm>
#include <cmath>
#include <ioj/layout/graph_view.hpp>
#include <set>

namespace ioj::layout {
using namespace lispb::schema;
namespace {
void append_edge_label(std::vector<std::string>& labels, std::string value) {
    if (!value.empty() && std::ranges::find(labels, value) == labels.end()) {
        labels.push_back(std::move(value));
    }
}
auto relationship_label(lispb::schema::SemanticRelationship const& relationship) -> std::string {
    auto result{std::string{codegen::semantic_relation_kind_name(relationship.kind)}};
    if (relationship.unit.has_value()) {
        result += " (";
        result += codegen::semantic_relation_unit_name(*relationship.unit);
        result += ')';
    }
    return result;
}

} // namespace

auto graph_edge_labels(TypeGraph const& types, TypeId const user, TypeId const dependency)
    -> std::vector<std::string> {
    auto const& definition{types.type(user).definition};
    std::vector<std::string> result;
    if (auto const* enumeration{std::get_if<EnumType>(&definition)}) {
        if (enumeration->underlying_type.has_value() &&
            enumeration->underlying_type->type == dependency) {
            append_edge_label(result, "underlying");
        }
    } else if (auto const* scalar{std::get_if<IntegerScalarType>(&definition)}) {
        if (scalar->relationship.has_value() && scalar->relationship->target.type == dependency) {
            append_edge_label(result, relationship_label(*scalar->relationship));
        }
    } else if (auto const* quantized{std::get_if<LinearQuantizedType>(&definition)}) {
        if (quantized->source.type == dependency) {
            append_edge_label(result, "quantises");
        }
    } else if (auto const* varint{std::get_if<IntegerVarintType>(&definition)}) {
        if (varint->source.type == dependency) {
            append_edge_label(result, "encodes");
        }
    } else if (auto const* optional{std::get_if<OptionalSentinelType>(&definition)}) {
        if (optional->source.type == dependency) {
            append_edge_label(result, "optional via " + optional->sentinel_name);
        }
    } else if (auto const* optional{std::get_if<OptionalPresenceBitType>(&definition)}) {
        if (optional->source.type == dependency) {
            append_edge_label(result, "optional via presence bit");
        }
    } else if (auto const* packed{std::get_if<PackedType>(&definition)}) {
        if (packed->storage_type.type == dependency) {
            append_edge_label(result, "storage");
        }
        for (auto const& segment : packed->segments) {
            if (auto const* field{std::get_if<PackedField>(&segment)}; field != nullptr) {
                if (field->semantic_type.type == dependency) {
                    append_edge_label(result, field->name);
                }
                if (field->relationship.has_value() &&
                    field->relationship->target.type == dependency) {
                    append_edge_label(result,
                                      field->name + " " + relationship_label(*field->relationship));
                }
            }
        }
    } else if (auto const* soa{std::get_if<SoaType>(&definition)}) {
        if (soa->equivalent_type.has_value() && soa->equivalent_type->type == dependency) {
            append_edge_label(result, "equivalent row");
        }
        for (auto const& column : soa->columns) {
            if (column.semantic_type.type == dependency) {
                append_edge_label(result, column.name);
            }
            if (column.nested_type == dependency) {
                append_edge_label(result, column.name + " nested");
            }
            if (column.relationship.has_value() && column.relationship->target.type == dependency) {
                append_edge_label(result,
                                  column.name + " " + relationship_label(*column.relationship));
            }
        }
    } else if (auto const* record{std::get_if<RecordType>(&definition)}) {
        for (auto const& member : record->members) {
            if (member.semantic_type.type == dependency) {
                append_edge_label(result, member.name);
            }
            if (member.relationship.has_value() && member.relationship->target.type == dependency) {
                append_edge_label(result,
                                  member.name + " " + relationship_label(*member.relationship));
            }
        }
    } else if (auto const* union_type{std::get_if<UnionType>(&definition)}) {
        for (auto const& alternative : union_type->alternatives) {
            if (alternative.semantic_type.type == dependency) {
                append_edge_label(result, alternative.name);
            }
        }
    } else if (auto const* tagged{std::get_if<TaggedUnionType>(&definition)}) {
        if (tagged->discriminant.type == dependency) {
            append_edge_label(result, "discriminates");
        }
        for (auto const& alternative : tagged->alternatives) {
            if (alternative.semantic_type.type == dependency) {
                append_edge_label(result, alternative.name + " [" + alternative.tag + "]");
            }
        }
    }
    if (result.empty()) {
        auto const& node{types.type(user)};
        auto const owner{node.owning_declaration.value_or(node.identity)};
        for (auto const& use : types.type_uses()) {
            if (use.declaration == owner && use.target.type == dependency) {
                append_edge_label(result, use.role);
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
            (scope.module && node.identity.module_name == *scope.module) ||
            std::ranges::find(scope.members, node.identity) != scope.members.end()) {
            members.insert(TypeId{static_cast<std::uint32_t>(index)});
        }
    }
    auto visible{members};
    for (auto const id : members) {
        for (auto const dependency : graph.type(id).dependencies) {
            visible.insert(dependency);
        }
    }
    std::map<TypeId, std::size_t> occurrence;
    for (auto const id : visible) {
        occurrence[id] = result.nodes.size();
        result.nodes.push_back({id, !members.contains(id)});
    }
    for (auto const id : members) {
        for (auto const dependency : graph.type(id).dependencies) {
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
