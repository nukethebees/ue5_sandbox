#pragma once

#include <ioj/layout/graph_point.hpp>

namespace ioj::layout {
struct GraphViewport {
    float zoom{1.0F};
    GraphPoint pan{};
};
}
