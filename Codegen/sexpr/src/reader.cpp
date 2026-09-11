#include <codegen/sexpr/reader.h>

#include <codegen/sexpr/lexer.h>

namespace codegen::sexpr {
namespace {

[[noreturn]] void
    fail(std::string_view const path, SourceSpan const& span, std::string const& message) {
    throw SourceError{path, span, message};
}

}

auto Form::is_list() const -> bool {
    return token.kind == TokenKind::left_parenthesis;
}

auto Form::head() const -> std::string_view {
    if (is_list() && !children.empty() && children.front().token.kind == TokenKind::atom) {
        return children.front().token.text;
    }
    return {};
}

auto read_form(std::span<Token const> const tokens, std::size_t& index) -> Form {
    if (index >= tokens.size()) {
        fail({}, SourceSpan{}, "expected an expression");
    }

    auto const token{tokens[index++]};
    if (token.kind == TokenKind::right_parenthesis || token.kind == TokenKind::end) {
        fail(token.span.path, token.span, "expected an expression");
    }

    Form result{token, {}, token};
    if (!result.is_list()) {
        return result;
    }

    while (index < tokens.size() && tokens[index].kind != TokenKind::right_parenthesis) {
        if (tokens[index].kind == TokenKind::end) {
            fail(token.span.path, token.span, "expected ')' after expression");
        }
        result.children.push_back(read_form(tokens, index));
    }
    if (index >= tokens.size()) {
        fail(token.span.path, token.span, "expected ')' after expression");
    }
    result.closing = tokens[index++];
    return result;
}

auto read_forms(std::string_view const path, std::string_view const source) -> std::vector<Form> {
    auto const tokens{lex(path, source)};
    std::vector<Form> result;
    std::size_t index{};
    while (tokens[index].kind != TokenKind::end) {
        result.push_back(read_form(tokens, index));
    }
    return result;
}

}
