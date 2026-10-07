#include "ast_parser.h"

#include <bitset>
#include <cmath>
#include <limits>

namespace ioj::levels::authoring::detail {
auto AstParser::record(s7::NodeIndex const node, RecordKind const kind, std::string const& path)
    -> std::vector<Field> {
    auto const children{list(node, path)};
    if (children.empty() || ast_.node(children[0]).kind != s7::NodeKind::Keyword ||
        ast_.text(children[0]) != to_serialized_string(kind)) {
        error(DiagnosticCode::ExpectedRecord,
              path,
              std::format("Expected a '{}' record", to_serialized_string(kind)));
        return {};
    }

    std::vector<Field> fields;
    std::bitset<ml::EnumTraits<Property>::values.size()> seen;
    auto const count{children.size()};
    for (std::size_t index{1}; index < count; index += 2) {
        auto const location{std::format("{}[{}]", path, index)};
        if (index + 1 == count) {
            error(DiagnosticCode::MissingValue, location, "Property has no value");
        }
        if (ast_.node(children[index]).kind != s7::NodeKind::Keyword) {
            error(DiagnosticCode::ExpectedKeyword, location, "Expected a property keyword");
            continue;
        }
        auto const name{ast_.text(children[index])};
        auto const property{try_parse_serialized_property(name)};
        if (!property) {
            error(DiagnosticCode::UnknownProperty,
                  location,
                  std::format("Unknown property ':{}'", name));
            continue;
        }
        auto const ordinal{static_cast<std::size_t>(*property)};
        if (seen.test(ordinal)) {
            error(DiagnosticCode::DuplicateProperty,
                  location,
                  std::format("Duplicate property ':{}'", name));
            continue;
        }
        seen.set(ordinal);
        if (index + 1 < count) {
            fields.push_back({*property, children[index + 1], std::format("{}.{}", path, name)});
        }
    }
    return fields;
}
auto AstParser::list(s7::NodeIndex const node, std::string const& path)
    -> std::span<s7::NodeIndex const> {
    if (ast_.node(node).kind != s7::NodeKind::List) {
        error(DiagnosticCode::InvalidType, path, "Expected a list");
        return {};
    }
    return ast_.children(node);
}
auto AstParser::symbol(s7::NodeIndex const node, std::string const& path) -> std::string {
    if (ast_.node(node).kind != s7::NodeKind::Symbol) {
        error(DiagnosticCode::InvalidType, path, "Expected a symbol");
        return {};
    }
    std::string result{ast_.text(node)};
    std::ranges::transform(result, result.begin(), ioj::to_ascii_lower);
    return result;
}
auto AstParser::string(s7::NodeIndex const node, std::string const& path) -> std::string {
    if (ast_.node(node).kind != s7::NodeKind::String) {
        error(DiagnosticCode::InvalidType, path, "Expected a string");
        return {};
    }
    return std::string{ast_.text(node)};
}
auto AstParser::number(s7::NodeIndex const node, std::string const& path) -> double {
    auto const& value{ast_.node(node)};
    switch (value.kind) {
        case s7::NodeKind::Integer:
            return static_cast<double>(value.integer);
        case s7::NodeKind::Ratio:
            return static_cast<double>(value.ratio.numerator) /
                   static_cast<double>(value.ratio.denominator);
        case s7::NodeKind::Real:
            return value.real;
        default:
            error(DiagnosticCode::InvalidType, path, "Expected a real number");
            return 0.0;
    }
}
auto AstParser::integer(s7::NodeIndex const node, std::string const& path) -> std::int32_t {
    auto const& value{ast_.node(node)};
    if (value.kind == s7::NodeKind::Integer) {
        if (value.integer >= std::numeric_limits<std::int32_t>::min() &&
            value.integer <= std::numeric_limits<std::int32_t>::max()) {
            return static_cast<std::int32_t>(value.integer);
        }
    } else if (value.kind == s7::NodeKind::Real) {
        if (std::isfinite(value.real) && std::trunc(value.real) == value.real &&
            value.real >= std::numeric_limits<std::int32_t>::min() &&
            value.real <= std::numeric_limits<std::int32_t>::max()) {
            return static_cast<std::int32_t>(value.real);
        }
    } else if (value.kind == s7::NodeKind::Ratio && value.ratio.denominator > 0 &&
               value.ratio.numerator % value.ratio.denominator == 0) {
        auto const quotient{value.ratio.numerator / value.ratio.denominator};
        if (quotient >= std::numeric_limits<std::int32_t>::min() &&
            quotient <= std::numeric_limits<std::int32_t>::max()) {
            return static_cast<std::int32_t>(quotient);
        }
    }
    error(DiagnosticCode::InvalidNumber, path, "Expected a 32-bit integer");
    return 0;
}
auto AstParser::float_number(s7::NodeIndex const node, std::string const& path) -> float {
    auto const value{number(node, path)};
    if (std::isfinite(value) && std::abs(value) > std::numeric_limits<float>::max()) {
        error(DiagnosticCode::InvalidNumber, path, "Number is outside the float range");
        return 0.0f;
    }
    return static_cast<float>(value);
}
void AstParser::require(std::span<Field const> const fields,
                        Property const property,
                        std::string const& path) {
    if (std::ranges::find(fields, property, &Field::property) == fields.end()) {
        error(DiagnosticCode::MissingProperty,
              std::format("{}.{}", path, to_serialized_string(property)),
              std::format("Missing property ':{}'", to_serialized_string(property)));
    }
}
void AstParser::unknown(Field const& field) {
    error(DiagnosticCode::UnknownProperty, field.node_path, "Property is not valid in this record");
}
void AstParser::error(DiagnosticCode const code, std::string path, std::string message) {
    errors_.push_back({code, std::move(path), std::move(message)});
}
}
