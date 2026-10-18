#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace lispb::codegen {

struct RenderContext {
    int indent_level{0};
    std::string_view indent_text{"    "};
    std::span<std::vector<std::string> const> include_groups;
    bool record_scope{false};

    auto indent() const -> RenderContext;
    auto apply_indent(std::string_view text) const -> std::string;
};

namespace ir {
void append_indent(std::string& output, RenderContext const& context);
void append_indented(std::string& output, std::string_view text, RenderContext const& context);
}

} // namespace lispb::codegen
