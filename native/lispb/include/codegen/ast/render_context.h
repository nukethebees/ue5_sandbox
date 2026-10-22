#pragma once

#include <codegen/ast/node_id.h>

#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace lispb::codegen {

struct RenderScope {
    std::vector<std::string_view> names;
    std::size_t namespace_depth{};
    bool namespace_scope{};
    bool opaque{};
};

// Own lookup state for one render; indentation contexts only borrow it.
struct RenderState {
    std::vector<std::string_view> namespaces;
    std::vector<RenderScope> scopes;
    std::map<std::string, RenderScope> namespace_scopes;
    std::map<std::string, NodeId> records;

    void append_name(std::string& output, std::span<std::string_view const> components) const;
};

class RenderScopeGuard {
  public:
    explicit RenderScopeGuard(RenderState& state);
    ~RenderScopeGuard();
    RenderScopeGuard(RenderScopeGuard const&) = delete;
    auto operator=(RenderScopeGuard const&) -> RenderScopeGuard& = delete;
  private:
    RenderState& state_;
    std::size_t namespace_size_;
    std::size_t scope_size_;
};

struct RenderContext {
    int indent_level{0};
    std::string_view indent_text{"    "};
    std::span<std::vector<std::string> const> include_groups;
    bool record_scope{false};
    RenderState* state{};

    auto indent() const -> RenderContext;
    auto apply_indent(std::string_view text) const -> std::string;
};

namespace ir {
void append_indent(std::string& output, RenderContext const& context);
void append_indented(std::string& output, std::string_view text, RenderContext const& context);
}

} // namespace lispb::codegen
