#pragma once

#include <ioj/layout/graph_projection.hpp>
#include <ioj/layout/graph_scope.hpp>
#include <ioj/layout/graph_viewport.hpp>

#include <lispb/schema/type_graph.h>

#include <span>
#include <string>
#include <vector>

namespace ioj::layout {

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
