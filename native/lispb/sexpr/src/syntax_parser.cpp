#include <codegen/sexpr/syntax_parser.h>

#include <codegen/sexpr/sources.h>
#include <codegen/sexpr/syntax.h>
#include <codegen/sexpr/token_kind.h>
#include <lispb/diagnostics.h>

#include <cassert>
#include <cstdint>
#include <span>

namespace lispb::codegen::sexpr {

SyntaxParser::SyntaxParser(Syntax& syntax, TokenRange tokens)
    : syntax_{syntax}
    , tokens_{tokens} {}
auto SyntaxParser::parse_for_editor() -> HandleRange {
    recover_ = true;
    return parse();
}
auto SyntaxParser::parse() -> HandleRange {
    std::vector<FormId> roots;
    while (syntax_.kind(TokenId{tokens_.offset + static_cast<std::uint32_t>(index_)}) !=
           TokenKind::end) {
        auto const token{TokenId{tokens_.offset + static_cast<std::uint32_t>(index_)}};
        if (recover_ && syntax_.kind(token) == TokenKind::right_parenthesis) {
            syntax_.sources().diagnostics().add(
                DiagnosticSeverity::error, syntax_.token_span(token), "unexpected ')'");
            ++index_;
        } else {
            roots.push_back(form());
        }
    }
    return syntax_.range(roots);
}
auto SyntaxParser::form() -> FormId {
    assert(index_ < tokens_.count);
    auto const token{TokenId{tokens_.offset + static_cast<std::uint32_t>(index_++)}};
    auto& diagnostics{syntax_.sources().diagnostics()};
    if (syntax_.kind(token) == TokenKind::right_parenthesis ||
        syntax_.kind(token) == TokenKind::end) {
        diagnostics.fail(syntax_.sources(), syntax_.token_span(token), "expected an expression");
    }
    if (syntax_.kind(token) != TokenKind::left_parenthesis) {
        return syntax_.add_leaf(token);
    }

    auto const child_begin{children_.size()};
    while (index_ < tokens_.count) {
        auto const next{TokenId{tokens_.offset + static_cast<std::uint32_t>(index_)}};
        if (syntax_.kind(next) == TokenKind::right_parenthesis) {
            ++index_;
            auto const result{syntax_.add_list(
                token, std::span{children_}.subspan(child_begin), syntax_.token_span(next))};
            children_.resize(child_begin);
            return result;
        }
        if (syntax_.kind(next) == TokenKind::end) {
            if (recover_) {
                diagnostics.add(DiagnosticSeverity::error,
                                syntax_.token_span(token),
                                "expected ')' after expression");
                auto const result{syntax_.add_list(
                    token, std::span{children_}.subspan(child_begin), syntax_.token_span(next))};
                children_.resize(child_begin);
                return result;
            }
            break;
        }
        auto const child{form()};
        children_.push_back(child);
    }
    diagnostics.fail(syntax_.sources(), syntax_.token_span(token), "expected ')' after expression");
}

}
