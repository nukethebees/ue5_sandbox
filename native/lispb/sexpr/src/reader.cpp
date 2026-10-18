#include <codegen/sexpr/reader.h>

#include <codegen/sexpr/indices.h>
#include <codegen/sexpr/lexer.h>
#include <codegen/sexpr/source_error.h>
#include <codegen/sexpr/syntax.h>
#include <codegen/sexpr/token_kind.h>
#include <codegen/sexpr/token_range.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace lispb::codegen::sexpr {
namespace {

auto read_form(Syntax& syntax,
               TokenRange const tokens,
               std::size_t& index,
               std::vector<FormId>& children) -> FormId {
    if (index >= tokens.count) {
        throw SourceError{syntax.sources(), {}, "expected an expression"};
    }

    auto const token{TokenId{tokens.offset + static_cast<std::uint32_t>(index++)}};
    if (syntax.kind(token) == TokenKind::right_parenthesis ||
        syntax.kind(token) == TokenKind::end) {
        throw SourceError{syntax.sources(), syntax.token_span(token), "expected an expression"};
    }
    if (syntax.kind(token) != TokenKind::left_parenthesis) {
        return syntax.add_leaf(token);
    }

    auto const child_begin{children.size()};
    while (index < tokens.count) {
        auto const next{TokenId{tokens.offset + static_cast<std::uint32_t>(index)}};
        if (syntax.kind(next) == TokenKind::right_parenthesis) {
            ++index;
            auto const result{syntax.add_list(
                token, std::span{children}.subspan(child_begin), syntax.token_span(next))};
            children.resize(child_begin);
            return result;
        }
        if (syntax.kind(next) == TokenKind::end) {
            break;
        }
        children.push_back(read_form(syntax, tokens, index, children));
    }

    throw SourceError{syntax.sources(), syntax.token_span(token), "expected ')' after expression"};
}

} // namespace

auto read_forms(Syntax& syntax, SourceFileId const file) -> HandleRange {
    if (auto const cached{syntax.cached_forms(file)}; cached.has_value()) {
        return *cached;
    }

    auto const tokens{lex(syntax, file)};
    std::vector<FormId> roots;
    std::vector<FormId> children;
    std::size_t index{};
    while (syntax.kind(TokenId{tokens.offset + static_cast<std::uint32_t>(index)}) !=
           TokenKind::end) {
        roots.push_back(read_form(syntax, tokens, index, children));
    }
    auto const result{syntax.range(roots)};
    syntax.cache_forms(file, result);
    return result;
}

} // namespace lispb::codegen::sexpr
