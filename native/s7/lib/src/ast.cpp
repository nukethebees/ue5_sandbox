#include <ioj/s7/ast.h>

#include <cassert>

namespace ioj::s7 {
auto Ast::node(NodeIndex const index) const -> Node const& {
    assert(index < nodes.size());
    return nodes[index];
}
auto Ast::children(NodeIndex const index) const -> std::span<NodeIndex const> {
    auto const& value{node(index)};
    assert(value.kind == NodeKind::List);
    assert(value.range.offset <= child_indices.size());
    assert(value.range.count <= child_indices.size() - value.range.offset);
    return std::span<NodeIndex const>{child_indices}.subspan(value.range.offset, value.range.count);
}
auto Ast::text(NodeIndex const index) const -> std::string_view {
    auto const& value{node(index)};
    assert(value.kind == NodeKind::Symbol || value.kind == NodeKind::Keyword ||
           value.kind == NodeKind::String);
    assert(value.range.offset <= text_bytes.size());
    assert(value.range.count <= text_bytes.size() - value.range.offset);
    return std::string_view{text_bytes}.substr(value.range.offset, value.range.count);
}
}
