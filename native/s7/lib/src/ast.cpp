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
    assert(value.offset <= child_indices.size());
    assert(value.count <= child_indices.size() - value.offset);
    return std::span<NodeIndex const>{child_indices}.subspan(value.offset, value.count);
}
auto Ast::text(NodeIndex const index) const -> std::string_view {
    auto const& value{node(index)};
    assert(value.kind == NodeKind::Symbol || value.kind == NodeKind::Keyword ||
           value.kind == NodeKind::String);
    assert(value.offset <= text_bytes.size());
    assert(value.count <= text_bytes.size() - value.offset);
    return std::string_view{text_bytes}.substr(value.offset, value.count);
}
auto Ast::boolean(NodeIndex const index) const -> bool {
    auto const& value{node(index)};
    assert(value.kind == NodeKind::Boolean);
    assert(value.count == 1 && value.offset < booleans.size());
    return booleans[value.offset] != 0;
}
auto Ast::integer(NodeIndex const index) const -> std::int64_t {
    auto const& value{node(index)};
    assert(value.kind == NodeKind::Integer);
    assert(value.count == 1 && value.offset < integers.size());
    return integers[value.offset];
}
auto Ast::ratio(NodeIndex const index) const -> Ratio {
    auto const& value{node(index)};
    assert(value.kind == NodeKind::Ratio);
    assert(value.count == 1 && value.offset < ratios.size());
    return ratios[value.offset];
}
auto Ast::real(NodeIndex const index) const -> double {
    auto const& value{node(index)};
    assert(value.kind == NodeKind::Real);
    assert(value.count == 1 && value.offset < reals.size());
    return reals[value.offset];
}
}
