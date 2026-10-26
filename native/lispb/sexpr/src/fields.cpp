#include <codegen/sexpr/fields.h>

#include <codegen/sexpr/indices.h>
#include <codegen/sexpr/parser.h>
#include <codegen/sexpr/source_span.h>
#include <codegen/sexpr/syntax.h>
#include <codegen/sexpr/token_kind.h>
#include <lispb/strings.h>

#include <algorithm>
#include <cstddef>
#include <format>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace lispb::codegen::sexpr {
Fields::Fields(Parser const& parser,
               FormId form,
               std::string_view const expected_head,
               std::size_t const positionals,
               bool const allow_references)
    : syntax_{parser.syntax()}
    , form_{form}
    , parser_{parser} {
    if (!syntax_.is_list(form) || syntax_.head(form) != expected_head) {
        parser_.fail(syntax_.token_span(form), std::format("expected '{}' form", expected_head));
    }
    if (syntax_.children(form).size() < positionals + 1) {
        parser_.fail(syntax_.token_span(form),
                     std::format("'{}' requires {} positional value{}",
                                 expected_head,
                                 positionals,
                                 positionals == 1 ? "" : "s"));
    }

    auto const count{syntax_.children(form).size()};
    for (std::size_t index{positionals + 1}; index < count;) {
        auto const child{syntax_.children(form)[index]};
        if (syntax_.kind(child) == TokenKind::keyword) {
            if (index + 1 >= count ||
                syntax_.kind(syntax_.children(form)[index + 1]) == TokenKind::keyword) {
                parser_.fail(syntax_.token_span(child),
                             std::format("property ':{}' requires a value", syntax_.text(child)));
            }
            if (std::ranges::find(properties_, syntax_.identifier(child)) != properties_.end()) {
                parser_.fail(syntax_.token_span(child),
                             std::format("duplicate property ':{}'", syntax_.text(child)));
            }
            properties_.push_back(syntax_.identifier(child));
            values_.push_back(syntax_.children(form)[index + 1]);
            property_spans_.push_back(syntax_.token_span(child));
            index += 2;
            continue;
        }
        if (!syntax_.is_list(child) &&
            !(allow_references && syntax_.text(child).starts_with('@'))) {
            parser_.fail(syntax_.token_span(child), "expected a property or nested declaration");
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
        parser_.fail(syntax_.token_span(form_),
                     std::format("missing required property ':{}'", name));
    }
    return *value;
}

auto Fields::declarations() const -> std::span<FormId const> {
    return declarations_;
}

void Fields::validate(std::initializer_list<std::string_view> const properties,
                      std::initializer_list<std::string_view> const declarations) const {
    validate_names(properties, declarations);
}

void Fields::validate_names(std::span<std::string_view const> const properties,
                            std::span<std::string_view const> const declarations) const {
    auto const count{properties_.size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const name{syntax_.strings().view(properties_[index])};
        if (std::ranges::find(properties, name) == properties.end()) {
            parser_.fail(property_spans_[index], std::format("unknown property ':{}'", name));
        }
    }

    for (auto const id : declarations_) {
        auto const declaration{id};
        if (!syntax_.is_list(declaration) && syntax_.text(declaration).starts_with('@')) {
            continue;
        }
        auto const head{syntax_.head(declaration)};
        if (head.empty()) {
            parser_.fail(syntax_.token_span(declaration),
                         "nested declaration must begin with a symbol");
        }
        if (std::ranges::find(declarations, head) == declarations.end()) {
            parser_.fail(syntax_.token_span(declaration),
                         std::format("unexpected nested declaration '{}'", head));
        }
    }
}

} // namespace lispb::codegen::sexpr
