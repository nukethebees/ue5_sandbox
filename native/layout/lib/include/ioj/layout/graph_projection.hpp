#pragma once

#include <ioj/layout/graph_edge.hpp>
#include <ioj/layout/graph_occurrence.hpp>

#include <vector>

namespace ioj::layout {
struct GraphProjection {
    std::vector<GraphOccurrence> nodes;
    std::vector<GraphEdge> edges;
};
}
