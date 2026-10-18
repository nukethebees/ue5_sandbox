#include <codegen/sexpr/sexpr_ir.h>

#include <codegen/sexpr/form_builder.h>
#include <lispb/strings.h>

#include <cassert>
#include <utility>

namespace lispb::codegen::sexpr {

SExprIR::SExprIR(Strings const& strings)
    : strings_{strings} {
    atoms = {text_expr(ExprKind::atom, "true"),
             text_expr(ExprKind::atom, "false"),
             text_expr(ExprKind::atom, "auto")};
}
auto SExprIR::atom(std::string_view value) -> Expr {
    if (value == "true") {
        return atoms.true_value;
    }
    if (value == "false") {
        return atoms.false_value;
    }
    if (value == "auto") {
        return atoms.automatic;
    }
    return text_expr(ExprKind::atom, value);
}
auto SExprIR::atom(StringId value) -> Expr {
    return text_expr(ExprKind::atom, value);
}
auto SExprIR::string(std::string_view value) -> Expr {
    return text_expr(ExprKind::string, value);
}
auto SExprIR::string(StringId value) -> Expr {
    return text_expr(ExprKind::string, value);
}
auto SExprIR::comment(std::string_view value) -> Expr {
    return text_expr(ExprKind::comment, value);
}
auto SExprIR::quote(Expr value) -> Expr {
    assert(value);
    auto const result{handle(ExprKind::quote, quotes_.size())};
    quotes_.push_back(value);
    return result;
}
auto SExprIR::list(std::span<Expr const> values) -> Expr {
    return save_list(values, {}, {});
}
auto SExprIR::list(std::initializer_list<Expr> values) -> Expr {
    return list(std::span<Expr const>{values});
}
auto SExprIR::form(std::string_view head, std::initializer_list<Expr> positional) -> FormBuilder {
    return FormBuilder{*this, atom(head), positional};
}
auto SExprIR::render(Expr value, Dialect dialect) const -> std::string {
    std::string result;
    append(result, value, 0, dialect);
    return result;
}
auto SExprIR::render_document(std::span<Expr const> roots, Dialect dialect) const -> std::string {
    std::string result;
    for (auto const root : roots) {
        append(result, root, 0, dialect);
        result += '\n';
    }
    return result;
}
auto SExprIR::render_document(std::initializer_list<Expr> roots, Dialect dialect) const
    -> std::string {
    return render_document(std::span<Expr const>{roots}, dialect);
}
auto SExprIR::text_expr(ExprKind kind, StringId value) -> Expr {
    auto const id{TextId{static_cast<std::uint32_t>(texts_.source.size())}};
    texts_.source.push_back(value);
    texts_.begin.push_back(0);
    texts_.size.push_back(0);
    return handle(kind, id.value);
}

auto SExprIR::handle(ExprKind kind, std::size_t index) -> Expr {
    assert(index < 0x01000000U);
    return Expr{(static_cast<std::uint32_t>(kind) << 24) | static_cast<std::uint32_t>(index)};
}
auto SExprIR::text_expr(ExprKind kind, std::string_view value) -> Expr {
    auto const id{TextId{static_cast<std::uint32_t>(texts_.begin.size())}};
    texts_.source.push_back({});
    texts_.begin.push_back(static_cast<std::uint32_t>(texts_.bytes.size()));
    texts_.size.push_back(static_cast<std::uint32_t>(value.size()));
    texts_.bytes.append(value);
    return handle(kind, id.value);
}
auto SExprIR::text(TextId id) const -> std::string_view {
    if (auto const source{texts_.source[id.value]}; source.value != 0) {
        return strings_.view(source);
    }
    return std::string_view{texts_.bytes}.substr(texts_.begin[id.value], texts_.size[id.value]);
}
auto SExprIR::save_children(std::span<Expr const> values) -> Range {
    Range const range{static_cast<std::uint32_t>(children_.size()),
                      static_cast<std::uint32_t>(values.size())};
    children_.insert(children_.end(), values.begin(), values.end());
    return range;
}
auto SExprIR::children(Range range) const -> std::span<Expr const> {
    return std::span{children_}.subspan(range.begin, range.count);
}
auto SExprIR::save_list(std::span<Expr const> header,
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
void SExprIR::append_string(std::string& output, std::string_view value, Dialect dialect) {
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
void SExprIR::append_values(std::string& output,
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
void SExprIR::append(std::string& output, Expr value, std::size_t depth, Dialect dialect) const {
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
            auto const rows{
                std::span{rows_}.subspan(lists_.rows[id.value].begin, lists_.rows[id.value].count)};
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
}
