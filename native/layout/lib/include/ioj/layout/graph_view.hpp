#pragma once

#include <lispb/schema/type_graph.h>

#include <array>
#include <span>

namespace ioj::layout {

struct GraphScope {
    // Empty means project overview; membership allows later scopes spanning modules.
    std::optional<std::string> module;
    std::vector<lispb::schema::TypeIdentity> members;
    auto operator==(GraphScope const&) const -> bool = default;
};
struct GraphOccurrence {
    lispb::schema::TypeId type;
    bool boundary{};
};
struct GraphEdge {
    std::size_t user{};
    std::size_t dependency{};
    std::vector<std::string> labels;
    std::string summary;
};
struct GraphProjection {
    std::vector<GraphOccurrence> nodes;
    std::vector<GraphEdge> edges;
};
using GraphPoint = std::array<float, 2>;
struct GraphViewport {
    float zoom{1.0F};
    GraphPoint pan{};
};

auto graph_edge_labels(lispb::schema::TypeGraph const& types,
                       lispb::schema::TypeId user,
                       lispb::schema::TypeId dependency) -> std::vector<std::string>;
auto graph_label_summary(std::span<std::string const> labels) -> std::string;
auto project_graph(lispb::schema::TypeGraph const& graph, GraphScope const& scope)
    -> GraphProjection;
auto layout_graph(GraphProjection const& graph, std::span<GraphPoint const> sizes)
    -> std::vector<GraphPoint>;
auto fit_graph(std::span<GraphPoint const> positions,
               std::span<GraphPoint const> sizes,
               GraphPoint viewport,
               float margin = 32.0F) -> GraphViewport;
auto graph_segment_hit(GraphPoint point, GraphPoint start, GraphPoint end, float tolerance) -> bool;

} // namespace ioj::layout
