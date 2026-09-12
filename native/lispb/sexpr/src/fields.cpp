#include <codegen/sexpr/fields.h>

#include <charconv>
#include <cmath>
#include <set>

namespace codegen::sexpr {
namespace {

[[noreturn]] void
    report(FailureHandler const failure, SourceSpan const& span, std::string const& message) {
    failure(span, message);
    std::terminate();
}

} // namespace

[[noreturn]] void throw_source_error(SourceSpan const& span, std::string const& message) {
    throw SourceError{span.path, span, message};
}

Fields::Fields(Form const& form,
               std::string_view const expected_head,
               std::size_t const positionals,
               FailureHandler const failure)
    : form_{form}
    , failure_{failure} {
    if (!form.is_list() || form.head() != expected_head) {
        report(failure_, form.token.span, "expected '" + std::string{expected_head} + "' form");
    }
    if (form.children.size() < positionals + 1) {
        report(failure_,
               form.token.span,
               "'" + std::string{expected_head} + "' requires " + std::to_string(positionals) +
                   " positional value" + (positionals == 1 ? "" : "s"));
    }

    for (std::size_t index{positionals + 1}; index < form.children.size();) {
        auto const& child{form.children[index]};
        if (child.token.kind == TokenKind::keyword) {
            if (index + 1 >= form.children.size() ||
                form.children[index + 1].token.kind == TokenKind::keyword) {
                report(failure_,
                       child.token.span,
                       "property ':" + child.token.text + "' requires a value");
            }
            if (!properties_.emplace(child.token.text, &form.children[index + 1]).second) {
                report(
                    failure_, child.token.span, "duplicate property ':" + child.token.text + "'");
            }
            property_spans_.emplace(child.token.text, child.token.span);
            index += 2;
            continue;
        }
        if (!child.is_list()) {
            report(failure_, child.token.span, "expected a property or nested declaration");
        }
        declarations_.push_back(&child);
        ++index;
    }
}

auto Fields::positional(std::size_t const index) const -> Form const& {
    return form_.children.at(index + 1);
}

auto Fields::optional(std::string_view const name) const -> Form const* {
    auto const found{properties_.find(name)};
    return found == properties_.end() ? nullptr : found->second;
}

auto Fields::required(std::string_view const name) const -> Form const& {
    auto const* value{optional(name)};
    if (value == nullptr) {
        report(
            failure_, form_.token.span, "missing required property ':" + std::string{name} + "'");
    }
    return *value;
}

auto Fields::declarations() const -> std::span<Form const* const> {
    return declarations_;
}

void Fields::validate(std::initializer_list<std::string_view> const properties,
                      std::initializer_list<std::string_view> const declarations) const {
    std::set<std::string_view> const allowed_properties{properties};
    for (auto const& [name, unused] : properties_) {
        static_cast<void>(unused);
        if (!allowed_properties.contains(name)) {
            report(failure_, property_spans_.at(name), "unknown property ':" + name + "'");
        }
    }

    std::set<std::string_view> const allowed_declarations{declarations};
    for (auto const* declaration : declarations_) {
        auto const head{declaration->head()};
        if (head.empty()) {
            report(
                failure_, declaration->token.span, "nested declaration must begin with a symbol");
        }
        if (!allowed_declarations.contains(head)) {
            report(failure_,
                   declaration->token.span,
                   "unexpected nested declaration '" + std::string{head} + "'");
        }
    }
}

auto text(Form const& form, std::string_view const purpose, FailureHandler const failure)
    -> std::string {
    if (form.is_list() ||
        (form.token.kind != TokenKind::atom && form.token.kind != TokenKind::string)) {
        report(failure, form.token.span, std::string{purpose} + " must be text or a symbol");
    }
    return form.token.text;
}

auto boolean(Form const& form, std::string_view const purpose, FailureHandler const failure)
    -> bool {
    if (!form.is_list() && form.token.kind == TokenKind::atom) {
        if (form.token.text == "true") {
            return true;
        }
        if (form.token.text == "false") {
            return false;
        }
    }
    report(failure, form.token.span, std::string{purpose} + " must be 'true' or 'false'");
}

auto integer(Form const& form, std::string_view const purpose, FailureHandler const failure)
    -> int {
    auto const value{text(form, purpose, failure)};
    int result{};
    auto const [position,
                error]{std::from_chars(value.data(), value.data() + value.size(), result)};
    if (error != std::errc{} || position != value.data() + value.size()) {
        report(failure, form.token.span, std::string{purpose} + " must be an integer");
    }
    return result;
}

auto number(Form const& form, std::string_view const purpose, FailureHandler const failure)
    -> double {
    auto const value{text(form, purpose, failure)};
    double result{};
    auto const [position,
                error]{std::from_chars(value.data(), value.data() + value.size(), result)};
    if (error != std::errc{} || position != value.data() + value.size() || !std::isfinite(result)) {
        report(failure, form.token.span, std::string{purpose} + " must be a finite number");
    }
    return result;
}

auto text_list(Form const& form, std::string_view const purpose, FailureHandler const failure)
    -> std::vector<std::string> {
    if (!form.is_list()) {
        report(failure, form.token.span, std::string{purpose} + " must be a list");
    }
    std::vector<std::string> result;
    result.reserve(form.children.size());
    for (auto const& child : form.children) {
        result.push_back(text(child, purpose, failure));
    }
    return result;
}

} // namespace codegen::sexpr
