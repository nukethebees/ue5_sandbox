#include "ast_materialization.h"

#include <s7.h>

#include <format>
#include <unordered_set>
#include <utility>

namespace ioj::s7::detail {
namespace {
class Materializer {
  public:
    Materializer(s7_scheme& scheme, AstLimits const limits)
        : scheme_{scheme}
        , limits_{limits} {}

    auto run(s7_pointer const value) -> AstResult {
        auto const root{copy(value, 0, "$")};
        if (!root) {
            return AstResult{std::unexpect, std::move(errors_)};
        }
        ast_.root = *root;
        return AstResult{std::in_place, std::move(ast_)};
    }
  private:
    auto fail(AstErrorCode const code, std::string const& path, std::string message)
        -> std::expected<NodeIndex, AstErrorCode> {
        errors_.push_back({code, path, std::move(message)});
        return std::unexpected{code};
    }

    auto copy(s7_pointer const value, std::uint32_t const depth, std::string const& path)
        -> std::expected<NodeIndex, AstErrorCode> {
        if (depth > limits_.max_depth || ast_.nodes.size() >= limits_.max_nodes) {
            return fail(AstErrorCode::LimitExceeded, path, "AST nesting or node limit exceeded");
        }

        auto const index{static_cast<NodeIndex>(ast_.nodes.size())};
        ast_.nodes.emplace_back();
        Node node;
        if (s7_is_null(&scheme_, value) || s7_is_pair(value)) {
            std::vector<NodeIndex> children;
            std::vector<s7_pointer> spine;
            auto tail{value};
            while (s7_is_pair(tail)) {
                if (!active_.insert(tail).second) {
                    return fail(AstErrorCode::CyclicStructure, path, "Cyclic Scheme structure");
                }
                spine.push_back(tail);
                if (edge_count_ >= limits_.max_child_indices) {
                    return fail(
                        AstErrorCode::LimitExceeded, path, "AST child index limit exceeded");
                }
                ++edge_count_;
                auto const child{
                    copy(s7_car(tail), depth + 1, std::format("{}[{}]", path, children.size()))};
                if (!child) {
                    return std::unexpected{child.error()};
                }
                children.push_back(*child);
                tail = s7_cdr(tail);
            }
            if (!s7_is_null(&scheme_, tail)) {
                return fail(AstErrorCode::ImproperList, path, "Expected a proper list");
            }
            for (auto const pair : spine) {
                active_.erase(pair);
            }
            node.offset = static_cast<std::uint32_t>(ast_.child_indices.size());
            node.count = static_cast<std::uint32_t>(children.size());
            ast_.child_indices.insert(ast_.child_indices.end(), children.begin(), children.end());
        } else if (s7_is_keyword(value) || s7_is_symbol(value) || s7_is_string(value)) {
            std::string_view text;
            if (s7_is_string(value)) {
                node.kind = NodeKind::String;
                text = {s7_string(value), static_cast<std::size_t>(s7_string_length(value))};
            } else {
                node.kind = s7_is_keyword(value) ? NodeKind::Keyword : NodeKind::Symbol;
                text = s7_symbol_name(value);
                if (node.kind == NodeKind::Keyword && text.starts_with(':')) {
                    text.remove_prefix(1);
                }
            }
            if (text.size() > limits_.max_text_bytes - ast_.text_bytes.size()) {
                return fail(AstErrorCode::LimitExceeded, path, "AST text limit exceeded");
            }
            node.offset = static_cast<std::uint32_t>(ast_.text_bytes.size());
            node.count = static_cast<std::uint32_t>(text.size());
            ast_.text_bytes.append(text);
        } else if (s7_is_boolean(value)) {
            node.kind = NodeKind::Boolean;
            node.offset = static_cast<std::uint32_t>(ast_.booleans.size());
            node.count = 1;
            ast_.booleans.push_back(static_cast<std::uint8_t>(s7_boolean(&scheme_, value)));
        } else if (s7_is_integer(value)) {
            node.kind = NodeKind::Integer;
            node.offset = static_cast<std::uint32_t>(ast_.integers.size());
            node.count = 1;
            ast_.integers.push_back(s7_integer(value));
        } else if (s7_is_rational(value)) {
            node.kind = NodeKind::Ratio;
            node.offset = static_cast<std::uint32_t>(ast_.ratios.size());
            node.count = 1;
            ast_.ratios.push_back(Ratio{s7_numerator(value), s7_denominator(value)});
        } else if (s7_is_real(value)) {
            node.kind = NodeKind::Real;
            node.offset = static_cast<std::uint32_t>(ast_.reals.size());
            node.count = 1;
            ast_.reals.push_back(s7_real(value));
        } else {
            return fail(AstErrorCode::UnsupportedValue, path, "Unsupported evaluated Scheme value");
        }

        ast_.nodes[index] = node;
        return index;
    }

    s7_scheme& scheme_;
    AstLimits limits_;
    Ast ast_{};
    AstDiagnostics errors_{};
    std::unordered_set<s7_pointer> active_{};
    std::uint32_t edge_count_{};
};
}

auto materialize_ast(s7_scheme& scheme, s7_cell* const value, AstLimits const limits) -> AstResult {
    return Materializer{scheme, limits}.run(value);
}
}
