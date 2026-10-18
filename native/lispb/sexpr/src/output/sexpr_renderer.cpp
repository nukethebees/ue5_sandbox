#include <codegen/sexpr/sexpr_renderer.h>

#include <codegen/sexpr/sexpr_ir.h>

#include <utility>

namespace lispb::codegen::sexpr {

SExprRenderer::SExprRenderer(SExprIR const& ir, Dialect dialect)
    : ir_{ir}
    , dialect_{dialect} {}

auto SExprRenderer::render(Expr value) const -> std::string {
    std::string result;
    append(result, value, 0);
    return result;
}
auto SExprRenderer::render_document(std::span<Expr const> roots) const -> std::string {
    std::string result;
    for (auto const root : roots) {
        append(result, root, 0);
        result += '\n';
    }
    return result;
}
auto SExprRenderer::render_document(std::initializer_list<Expr> roots) const -> std::string {
    return render_document(std::span<Expr const>{roots});
}
void SExprRenderer::append_string(std::string& output, std::string_view value) const {
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
                if (dialect_ == Dialect::scheme && static_cast<unsigned char>(character) < 32) {
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
void SExprRenderer::append_values(std::string& output,
                                  std::span<Expr const> values,
                                  std::size_t depth) const {
    bool first{true};
    for (auto const value : values) {
        if (!first) {
            output += ' ';
        }
        append(output, value, depth);
        first = false;
    }
}
void SExprRenderer::append(std::string& output, Expr value, std::size_t depth) const {
    switch (value.kind()) {
        case ExprKind::atom:
            output += ir_.text(output_detail::TextId{value.index()});
            break;
        case ExprKind::string:
            append_string(output, ir_.text(output_detail::TextId{value.index()}));
            break;
        case ExprKind::comment:
            output += ";; ";
            output += ir_.text(output_detail::TextId{value.index()});
            break;
        case ExprKind::quote:
            output += '\'';
            append(output, ir_.quotes_[value.index()], depth);
            break;
        case ExprKind::list: {
            auto const id{output_detail::ListId{value.index()}};
            output += '(';
            append_values(output, ir_.children(ir_.lists_.header[id.value]), depth);
            auto const rows{std::span{ir_.rows_}.subspan(ir_.lists_.rows[id.value].begin,
                                                         ir_.lists_.rows[id.value].count)};
            for (auto const row : rows) {
                output += '\n';
                output.append((depth + 1) * 2, ' ');
                append_values(output, ir_.children(row), depth + 1);
            }
            if (!rows.empty() && ir_.children(rows.back()).back().kind() == ExprKind::comment) {
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
