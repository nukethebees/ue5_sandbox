#include <ioj/s7/ast.h>

#include <cassert>

namespace ioj::s7 {
namespace {
void assert_range(Node const& node, std::size_t const size) {
    assert(node.offset <= size);
    assert(node.count <= size - node.offset);
}
template <typename T>
auto scalar(Node const& node, NodeKind const kind, std::vector<T> const& storage) -> T {
    assert(node.kind == kind);
    assert(node.count == 1);
    assert_range(node, storage.size());
    return storage[node.offset];
}
}

auto Ast::node(NodeIndex const index) const -> Node const& {
    assert(index < nodes.size());
    return nodes[index];
}
auto Ast::children(NodeIndex const index) const -> std::span<NodeIndex const> {
    auto const& value{node(index)};
    assert(value.kind == NodeKind::List);
    assert_range(value, child_indices.size());
    return std::span<NodeIndex const>{child_indices}.subspan(value.offset, value.count);
}
auto Ast::text(NodeIndex const index) const -> std::string_view {
    auto const& value{node(index)};
    assert(value.kind == NodeKind::Symbol || value.kind == NodeKind::Keyword ||
           value.kind == NodeKind::String);
    assert_range(value, text_bytes.size());
    return std::string_view{text_bytes}.substr(value.offset, value.count);
}
auto Ast::boolean(NodeIndex const index) const -> bool {
    return scalar(node(index), NodeKind::Boolean, booleans) != 0;
}
auto Ast::integer(NodeIndex const index) const -> std::int64_t {
    return scalar(node(index), NodeKind::Integer, integers);
}
auto Ast::ratio(NodeIndex const index) const -> Ratio {
    return scalar(node(index), NodeKind::Ratio, ratios);
}
auto Ast::real(NodeIndex const index) const -> double {
    return scalar(node(index), NodeKind::Real, reals);
}
}
