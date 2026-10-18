#include <codegen/sexpr/fields.h>

#include <codegen/sexpr/failure_handler.h>
#include <codegen/sexpr/indices.h>
#include <codegen/sexpr/source_error.h>
#include <codegen/sexpr/source_span.h>
#include <codegen/sexpr/sources.h>
#include <codegen/sexpr/syntax.h>
#include <codegen/sexpr/token_kind.h>
#include <lispb/strings.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <exception>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace lispb::codegen::sexpr {
namespace {

[[noreturn]] void report(FailureHandler const failure,
                         Sources const& sources,
                         SourceSpan const span,
                         std::string const& message) {
    failure(sources, span, message);
    std::terminate();
}

} // namespace

[[noreturn]] void
    throw_source_error(Sources const& sources, SourceSpan const& span, std::string const& message) {
    throw SourceError{sources, span, message};
}

Fields::Fields(Syntax const& syntax,
               FormId form,
               std::string_view const expected_head,
               std::size_t const positionals,
               FailureHandler const failure)
    : syntax_{syntax}
    , form_{form}
    , failure_{failure} {
    auto const& sources{syntax_.sources()};
    if (!syntax_.is_list(form) || syntax_.head(form) != expected_head) {
        report(failure_,
               sources,
               syntax_.token_span(form),
               "expected '" + std::string{expected_head} + "' form");
    }
    if (syntax_.children(form).size() < positionals + 1) {
        report(failure_,
               sources,
               syntax_.token_span(form),
               "'" + std::string{expected_head} + "' requires " + std::to_string(positionals) +
                   " positional value" + (positionals == 1 ? "" : "s"));
    }

    auto const count{syntax_.children(form).size()};
    for (std::size_t index{positionals + 1}; index < count;) {
        auto const child{syntax_.children(form)[index]};
        if (syntax_.kind(child) == TokenKind::keyword) {
            if (index + 1 >= count ||
                syntax_.kind(syntax_.children(form)[index + 1]) == TokenKind::keyword) {
                report(failure_,
                       sources,
                       syntax_.token_span(child),
                       "property ':" + std::string{syntax_.text(child)} + "' requires a value");
            }
            if (std::ranges::find(properties_, syntax_.identifier(child)) != properties_.end()) {
                report(failure_,
                       sources,
                       syntax_.token_span(child),
                       "duplicate property ':" + std::string{syntax_.text(child)} + "'");
            }
            properties_.push_back(syntax_.identifier(child));
            values_.push_back(syntax_.children(form)[index + 1]);
            property_spans_.push_back(syntax_.token_span(child));
            index += 2;
            continue;
        }
        if (!syntax_.is_list(child)) {
            report(failure_,
                   sources,
                   syntax_.token_span(child),
                   "expected a property or nested declaration");
        }
        declarations_.push_back(child);
        ++index;
    }
}

auto Fields::positional(std::size_t const index) const -> FormId {
    return syntax_.child(form_, index + 1);
}

auto Fields::optional(std::string_view const name) const -> std::optional<FormId> {
    auto const count{properties_.size()};
    for (std::size_t index{}; index < count; ++index) {
        if (syntax_.strings().view(properties_[index]) == name) {
            return values_[index];
        }
    }
    return std::nullopt;
}

auto Fields::required(std::string_view const name) const -> FormId {
    auto const value{optional(name)};
    if (!value.has_value()) {
        report(failure_,
               syntax_.sources(),
               syntax_.token_span(form_),
               "missing required property ':" + std::string{name} + "'");
    }
    return *value;
}

auto Fields::declarations() const -> std::span<FormId const> {
    return declarations_;
}

void Fields::validate(std::initializer_list<std::string_view> const properties,
                      std::initializer_list<std::string_view> const declarations) const {
    auto const count{properties_.size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const name{syntax_.strings().view(properties_[index])};
        if (std::ranges::find(properties, name) == properties.end()) {
            report(failure_,
                   syntax_.sources(),
                   property_spans_[index],
                   "unknown property ':" + std::string{name} + "'");
        }
    }

    for (auto const id : declarations_) {
        auto const declaration{id};
        auto const head{syntax_.head(declaration)};
        if (head.empty()) {
            report(failure_,
                   syntax_.sources(),
                   syntax_.token_span(declaration),
                   "nested declaration must begin with a symbol");
        }
        if (std::ranges::find(declarations, head) == declarations.end()) {
            report(failure_,
                   syntax_.sources(),
                   syntax_.token_span(declaration),
                   "unexpected nested declaration '" + std::string{head} + "'");
        }
    }
}

auto text(Syntax const& syntax,
          FormId const form,
          std::string_view const purpose,
          FailureHandler const failure) -> std::string_view {
    if (syntax.is_list(form) ||
        (syntax.kind(form) != TokenKind::atom && syntax.kind(form) != TokenKind::string)) {
        report(failure,
               syntax.sources(),
               syntax.token_span(form),
               std::string{purpose} + " must be text or a symbol");
    }
    return syntax.text(form);
}

auto raw_text(Syntax const& syntax,
              FormId const form,
              std::string_view const expected_tag,
              std::string_view const purpose,
              FailureHandler const failure) -> std::string_view {
    if (syntax.is_list(form) || syntax.kind(form) != TokenKind::raw_literal) {
        report(failure,
               syntax.sources(),
               syntax.token_span(form),
               std::string{purpose} + " must be a #" + std::string{expected_tag} + " raw literal");
    }
    if (syntax.tag(form) != expected_tag) {
        report(failure,
               syntax.sources(),
               syntax.token_span(form),
               std::string{purpose} + " requires a #" + std::string{expected_tag} +
                   " raw literal; got #" + std::string{syntax.tag(form)});
    }
    return syntax.text(form);
}

auto boolean(Syntax const& syntax,
             FormId const form,
             std::string_view const purpose,
             FailureHandler const failure) -> bool {
    if (!syntax.is_list(form) && syntax.kind(form) == TokenKind::atom) {
        if (syntax.text(form) == "true") {
            return true;
        }
        if (syntax.text(form) == "false") {
            return false;
        }
    }
    report(failure,
           syntax.sources(),
           syntax.token_span(form),
           std::string{purpose} + " must be 'true' or 'false'");
}

auto integer(Syntax const& syntax,
             FormId const form,
             std::string_view const purpose,
             FailureHandler const failure) -> int {
    auto const value{text(syntax, form, purpose, failure)};
    int result{};
    auto const [position,
                error]{std::from_chars(value.data(), value.data() + value.size(), result)};
    if (error != std::errc{} || position != value.data() + value.size()) {
        report(failure,
               syntax.sources(),
               syntax.token_span(form),
               std::string{purpose} + " must be an integer");
    }
    return result;
}

auto number(Syntax const& syntax,
            FormId const form,
            std::string_view const purpose,
            FailureHandler const failure) -> double {
    auto const value{text(syntax, form, purpose, failure)};
    double result{};
    auto const [position,
                error]{std::from_chars(value.data(), value.data() + value.size(), result)};
    if (error != std::errc{} || position != value.data() + value.size() || !std::isfinite(result)) {
        report(failure,
               syntax.sources(),
               syntax.token_span(form),
               std::string{purpose} + " must be a finite number");
    }
    return result;
}

auto text_list(Syntax const& syntax,
               FormId const form,
               std::string_view const purpose,
               FailureHandler const failure) -> std::vector<std::string> {
    if (!syntax.is_list(form)) {
        report(failure,
               syntax.sources(),
               syntax.token_span(form),
               std::string{purpose} + " must be a list");
    }
    std::vector<std::string> result;
    result.reserve(syntax.children(form).size());
    for (auto const child : syntax.children(form)) {
        result.emplace_back(text(syntax, child, purpose, failure));
    }
    return result;
}

} // namespace lispb::codegen::sexpr
