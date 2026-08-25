#pragma once

#include <codegen/ast/include.h>

#include <string>
#include <string_view>
#include <vector>

namespace codegen {

struct RenderContext {
    int indent_level{0};
    std::string indent_text{"    "};
    std::vector<std::vector<Include>> include_groups;

    auto indent() const -> RenderContext;
    auto apply_indent(std::string_view text) const -> std::string;
};

} // namespace codegen
