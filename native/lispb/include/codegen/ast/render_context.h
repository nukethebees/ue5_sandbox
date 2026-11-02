#pragma once

#include <codegen/ast/type_destination.h>

#include <span>
#include <string>
#include <vector>

namespace lispb::codegen {
struct RenderContext {
    int indent_level{0};
    std::span<std::vector<std::string> const> include_groups;
    TypeDestination return_destination{};

    auto indent() const -> RenderContext;
};
}
