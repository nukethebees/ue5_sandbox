#include <codegen/sexpr/parser.h>

#include <codegen/sexpr/sources.h>
#include <codegen/sexpr/syntax.h>
#include <codegen/sexpr/token_kind.h>
#include <lispb/diagnostics.h>
#include <lispb/strings.h>

#include <charconv>
#include <cmath>
#include <string>
#include <system_error>

namespace lispb::codegen::sexpr {

Parser::Parser(Syntax& syntax)
    : syntax_{syntax} {}
auto Parser::syntax() const -> Syntax& {
    return syntax_;
}
void Parser::fail(FormId form, std::string_view message) const {
    fail(syntax_.token_span(form), message);
}
void Parser::fail(SourceSpan span, std::string_view message) const {
    syntax_.sources().diagnostics().fail(syntax_.sources(), span, message);
}
auto Parser::text_id(FormId form, std::string_view purpose) const -> StringId {
    static_cast<void>(text(form, purpose));
    return syntax_.text_id(form);
}
auto Parser::identifier(FormId form, std::string_view purpose) const -> IdentifierId {
    auto const value{text_id(form, purpose)};
    return syntax_.kind(form) == TokenKind::atom ? syntax_.identifier(form)
                                                 : syntax_.strings().intern(value);
}

auto Parser::text(FormId const form, std::string_view const purpose) const -> std::string_view {
    if (syntax_.is_list(form) ||
        (syntax_.kind(form) != TokenKind::atom && syntax_.kind(form) != TokenKind::string)) {
        fail(syntax_.token_span(form), std::string{purpose} + " must be text or a symbol");
    }
    return syntax_.text(form);
}

auto Parser::boolean(FormId const form, std::string_view const purpose) const -> bool {
    if (!syntax_.is_list(form) && syntax_.kind(form) == TokenKind::atom) {
        if (syntax_.text(form) == "true") {
            return true;
        }
        if (syntax_.text(form) == "false") {
            return false;
        }
    }
    fail(syntax_.token_span(form), std::string{purpose} + " must be 'true' or 'false'");
}

auto Parser::integer(FormId const form, std::string_view const purpose) const -> int {
    auto const value{text(form, purpose)};
    int result{};
    auto const [position,
                error]{std::from_chars(value.data(), value.data() + value.size(), result)};
    if (error != std::errc{} || position != value.data() + value.size()) {
        fail(syntax_.token_span(form), std::string{purpose} + " must be an integer");
    }
    return result;
}

auto Parser::number(FormId const form, std::string_view const purpose) const -> double {
    auto const value{text(form, purpose)};
    double result{};
    auto const [position,
                error]{std::from_chars(value.data(), value.data() + value.size(), result)};
    if (error != std::errc{} || position != value.data() + value.size() || !std::isfinite(result)) {
        fail(syntax_.token_span(form), std::string{purpose} + " must be a finite number");
    }
    return result;
}

}
