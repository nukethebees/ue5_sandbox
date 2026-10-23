#pragma once

#include <span>
#include <string>
#include <vector>

namespace lispb::codegen {
struct RenderContext {
    int indent_level{0};
    std::span<std::vector<std::string> const> include_groups;

    auto indent() const -> RenderContext;
};
}
