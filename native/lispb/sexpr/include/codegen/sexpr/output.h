#pragma once

#include <lispb/index.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lispb::codegen::sexpr {

enum class ExprKind : std::uint8_t { invalid, atom, string, list, quote, comment };
enum class Dialect : std::uint8_t { lispb, scheme };

struct Expr {
    std::uint32_t value{};
    explicit operator bool() const { return value != 0; }
    auto kind() const -> ExprKind { return static_cast<ExprKind>(value >> 24); }
    auto index() const -> std::uint32_t { return value & 0x00ffffffU; }
    auto operator==(Expr const&) const -> bool = default;
};
static_assert(sizeof(Expr) == 4);

namespace output_detail {
struct Range {
    std::uint32_t begin{};
    std::uint32_t count{};
};
struct Texts {
    std::vector<std::uint32_t> begin;
    std::vector<std::uint32_t> size;
    std::string bytes;
};
struct Lists {
    std::vector<Range> header;
    std::vector<Range> rows;
};
using TextId = Index<Texts>;
using ListId = Index<Lists>;
}

class SExprIR;

class FormBuilder {
  public:
    auto field(std::string_view key, Expr value) -> FormBuilder&;
    auto add(Expr child) -> FormBuilder&;
    // Publish the collected rows; all resulting handles belong to this builder's IR.
    auto build() -> Expr;
  private:
    friend class SExprIR;
    FormBuilder(SExprIR& ir, Expr head, std::initializer_list<Expr> positional);
    SExprIR& ir_;
    std::vector<Expr> header_;
    std::vector<Expr> children_;
    std::vector<output_detail::Range> rows_;
};

// Handles belong to this IR and remain valid as it grows. Atoms must be valid source tokens;
// comments must be single lines and belong to form rows or document roots.
class SExprIR {
  public:
    auto atom(std::string_view value) -> Expr { return text_expr(ExprKind::atom, value); }
    auto string(std::string_view value) -> Expr { return text_expr(ExprKind::string, value); }
    auto comment(std::string_view value) -> Expr { return text_expr(ExprKind::comment, value); }
    auto quote(Expr value) -> Expr {
        assert(value);
        auto const result{handle(ExprKind::quote, quotes_.size())};
        quotes_.push_back(value);
        return result;
    }
    auto list(std::span<Expr const> values) -> Expr { return save_list(values, {}, {}); }
    auto list(std::initializer_list<Expr> values) -> Expr {
        return list(std::span<Expr const>{values});
    }
    auto form(std::string_view head, std::initializer_list<Expr> positional = {}) -> FormBuilder {
        return FormBuilder{*this, atom(head), positional};
    }
    auto render(Expr value, Dialect dialect = Dialect::lispb) const -> std::string {
        std::string result;
        append(result, value, 0, dialect);
        return result;
    }
    auto render_document(std::span<Expr const> roots, Dialect dialect = Dialect::lispb) const
        -> std::string {
        std::string result;
        for (auto const root : roots) {
            append(result, root, 0, dialect);
            result += '\n';
        }
        return result;
    }
    auto render_document(std::initializer_list<Expr> roots, Dialect dialect = Dialect::lispb) const
        -> std::string {
        return render_document(std::span<Expr const>{roots}, dialect);
    }
  private:
    friend class FormBuilder;
    using Range = output_detail::Range;
    using TextId = output_detail::TextId;
    using ListId = output_detail::ListId;

    static auto handle(ExprKind kind, std::size_t index) -> Expr {
        assert(index < 0x01000000U);
        return Expr{(static_cast<std::uint32_t>(kind) << 24) | static_cast<std::uint32_t>(index)};
    }
    auto text_expr(ExprKind kind, std::string_view value) -> Expr {
        auto const id{TextId{static_cast<std::uint32_t>(texts_.begin.size())}};
        texts_.begin.push_back(static_cast<std::uint32_t>(texts_.bytes.size()));
        texts_.size.push_back(static_cast<std::uint32_t>(value.size()));
        texts_.bytes.append(value);
        return handle(kind, id.value);
    }
    auto text(TextId id) const -> std::string_view {
        return std::string_view{texts_.bytes}.substr(texts_.begin[id.value], texts_.size[id.value]);
    }
    auto save_children(std::span<Expr const> values) -> Range {
        Range const range{static_cast<std::uint32_t>(children_.size()),
                          static_cast<std::uint32_t>(values.size())};
        children_.insert(children_.end(), values.begin(), values.end());
        return range;
    }
    auto children(Range range) const -> std::span<Expr const> {
        return std::span{children_}.subspan(range.begin, range.count);
    }
    auto save_list(std::span<Expr const> header,
                   std::span<Expr const> values,
                   std::span<Range const> rows) -> Expr {
        auto const id{ListId{static_cast<std::uint32_t>(lists_.header.size())}};
        auto const saved_header{save_children(header)};
        auto const saved_values{save_children(values)};
        Range const saved_rows{static_cast<std::uint32_t>(rows_.size()),
                               static_cast<std::uint32_t>(rows.size())};
        for (auto const row : rows) {
            rows_.push_back({saved_values.begin + row.begin, row.count});
        }
        lists_.header.push_back(saved_header);
        lists_.rows.push_back(saved_rows);
        return handle(ExprKind::list, id.value);
    }
    static void append_string(std::string& output, std::string_view value, Dialect dialect) {
        output += '"';
        for (auto const character : value) {
            switch (character) {
                case '\\':
                    output += "\\\\";
                    break;
                case '"':
                    output += "\\\"";
                    break;
                case '\n':
                    output += "\\n";
                    break;
                case '\r':
                    output += "\\r";
                    break;
                case '\t':
                    output += "\\t";
                    break;
                default:
                    if (dialect == Dialect::scheme && static_cast<unsigned char>(character) < 32) {
                        constexpr std::string_view hex{"0123456789abcdef"};
                        auto const byte{static_cast<unsigned char>(character)};
                        output += "\\x";
                        output += hex[byte >> 4];
                        output += hex[byte & 15];
                        output += ';';
                    } else {
                        output += character;
                    }
                    break;
            }
        }
        output += '"';
    }
    void append_values(std::string& output,
                       std::span<Expr const> values,
                       std::size_t depth,
                       Dialect dialect) const {
        bool first{true};
        for (auto const value : values) {
            if (!first) {
                output += ' ';
            }
            append(output, value, depth, dialect);
            first = false;
        }
    }
    void append(std::string& output, Expr value, std::size_t depth, Dialect dialect) const {
        switch (value.kind()) {
            case ExprKind::atom:
                output += text(TextId{value.index()});
                break;
            case ExprKind::string:
                append_string(output, text(TextId{value.index()}), dialect);
                break;
            case ExprKind::comment:
                output += ";; ";
                output += text(TextId{value.index()});
                break;
            case ExprKind::quote:
                output += '\'';
                append(output, quotes_[value.index()], depth, dialect);
                break;
            case ExprKind::list: {
                auto const id{ListId{value.index()}};
                output += '(';
                append_values(output, children(lists_.header[id.value]), depth, dialect);
                auto const rows{std::span{rows_}.subspan(lists_.rows[id.value].begin,
                                                         lists_.rows[id.value].count)};
                for (auto const row : rows) {
                    output += '\n';
                    output.append((depth + 1) * 2, ' ');
                    append_values(output, children(row), depth + 1, dialect);
                }
                if (!rows.empty() && children(rows.back()).back().kind() == ExprKind::comment) {
                    output += '\n';
                    output.append(depth * 2, ' ');
                }
                output += ')';
                break;
            }
            case ExprKind::invalid:
                std::unreachable();
        }
    }

    output_detail::Texts texts_;
    output_detail::Lists lists_;
    std::vector<Expr> children_;
    std::vector<Range> rows_;
    std::vector<Expr> quotes_;
};

inline FormBuilder::FormBuilder(SExprIR& ir, Expr head, std::initializer_list<Expr> positional)
    : ir_{ir}
    , header_{head} {
    header_.insert(header_.end(), positional.begin(), positional.end());
}
inline auto FormBuilder::field(std::string_view key, Expr value) -> FormBuilder& {
    rows_.push_back({static_cast<std::uint32_t>(children_.size()), 2});
    children_.push_back(ir_.atom(":" + std::string{key}));
    children_.push_back(value);
    return *this;
}
inline auto FormBuilder::add(Expr child) -> FormBuilder& {
    rows_.push_back({static_cast<std::uint32_t>(children_.size()), 1});
    children_.push_back(child);
    return *this;
}
inline auto FormBuilder::build() -> Expr {
    return ir_.save_list(header_, children_, rows_);
}

} // namespace lispb::codegen::sexpr
