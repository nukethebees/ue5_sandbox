#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace ioj::layout {
struct GraphEdge {
    std::size_t user{};
    std::size_t dependency{};
    std::vector<std::string> labels;
    std::string summary;
};
}
