#include "ast_parser.h"

#include <bitset>
#include <cmath>
#include <limits>

namespace ioj::levels::authoring::detail {
auto AstParser::record(s7::NodeIndex const node,
                       RecordKind const kind,
                       std::string_view const node_path) -> std::vector<Field> {
    auto const children{list(node, node_path)};
    if (children.empty() || ast_.node(children[0]).kind != s7::NodeKind::Keyword ||
        ast_.text(children[0]) != to_serialized_string(kind)) {
        error(DiagnosticCode::ExpectedRecord,
              node_path,
              std::format("Expected a '{}' record", to_serialized_string(kind)));
        return {};
    }

    std::vector<Field> fields;
    std::bitset<ml::EnumTraits<Property>::values.size()> seen;
    auto const count{children.size()};
    for (std::size_t index{1}; index < count; index += 2) {
        auto const location{std::format("{}[{}]", node_path, index)};
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
            fields.emplace_back(
                *property, children[index + 1], std::format("{}.{}", node_path, name));
        }
    }
    return fields;
}
auto AstParser::list(s7::NodeIndex const node, std::string_view const node_path)
    -> std::span<s7::NodeIndex const> {
    if (ast_.node(node).kind != s7::NodeKind::List) {
        error(DiagnosticCode::InvalidType, node_path, "Expected a list");
        return {};
    }
    return ast_.children(node);
}
auto AstParser::symbol(s7::NodeIndex const node, std::string_view const node_path) -> std::string {
    if (ast_.node(node).kind != s7::NodeKind::Symbol) {
        error(DiagnosticCode::InvalidType, node_path, "Expected a symbol");
        return {};
    }
    std::string result{ast_.text(node)};
    std::ranges::transform(result, result.begin(), ioj::to_ascii_lower);
    return result;
}
auto AstParser::team(s7::NodeIndex const node, std::string_view const node_path) -> TeamId {
    auto const text{symbol(node, node_path)};
    auto const value{try_parse_serialized_team_id(text)};
    if (!value && ast_.node(node).kind == s7::NodeKind::Symbol) {
        error(DiagnosticCode::UnsupportedTeamId, node_path, std::format("Unknown team '{}'", text));
    }
    return value.value_or(TeamId::White);
}
auto AstParser::archetype(s7::NodeIndex const node, std::string_view const node_path)
    -> EntityArchetype {
    auto const text{symbol(node, node_path)};
    auto const value{try_parse_serialized_entity_archetype(text)};
    if (!value && ast_.node(node).kind == s7::NodeKind::Symbol) {
        error(DiagnosticCode::UnsupportedArchetype,
              node_path,
              std::format("Unknown archetype '{}'", text));
    }
    return value.value_or(EntityArchetype::PlayerFighter);
}
auto AstParser::teams(s7::NodeIndex const node, std::string_view const node_path)
    -> std::vector<TeamId> {
    std::vector<TeamId> result;
    auto const values{list(node, node_path)};
    auto const count{values.size()};
    for (std::size_t index{}; index < count; ++index) {
        result.push_back(team(values[index], std::format("{}[{}]", node_path, index)));
    }
    return result;
}

auto AstParser::string(s7::NodeIndex const node, std::string_view const node_path) -> std::string {
    if (ast_.node(node).kind != s7::NodeKind::String) {
        error(DiagnosticCode::InvalidType, node_path, "Expected a string");
        return {};
    }
    return std::string{ast_.text(node)};
}
auto AstParser::number(s7::NodeIndex const node, std::string_view const node_path) -> double {
    auto const& value{ast_.node(node)};
    switch (value.kind) {
        case s7::NodeKind::Integer:
            return static_cast<double>(ast_.integer(node));
        case s7::NodeKind::Ratio: {
            auto const ratio{ast_.ratio(node)};
            return static_cast<double>(ratio.numerator) / static_cast<double>(ratio.denominator);
        }
        case s7::NodeKind::Real:
            return ast_.real(node);
        default:
            error(DiagnosticCode::InvalidType, node_path, "Expected a real number");
            return 0.0;
    }
}
auto AstParser::integer(s7::NodeIndex const node, std::string_view const node_path)
    -> std::int32_t {
    auto const& value{ast_.node(node)};
    if (value.kind == s7::NodeKind::Integer) {
        auto const integer{ast_.integer(node)};
        if (integer >= std::numeric_limits<std::int32_t>::min() &&
            integer <= std::numeric_limits<std::int32_t>::max()) {
            return static_cast<std::int32_t>(integer);
        }
    } else if (value.kind == s7::NodeKind::Real) {
        auto const real{ast_.real(node)};
        if (std::isfinite(real) && std::trunc(real) == real &&
            real >= std::numeric_limits<std::int32_t>::min() &&
            real <= std::numeric_limits<std::int32_t>::max()) {
            return static_cast<std::int32_t>(real);
        }
    } else if (value.kind == s7::NodeKind::Ratio) {
        auto const ratio{ast_.ratio(node)};
        if (ratio.denominator > 0 && ratio.numerator % ratio.denominator == 0) {
            auto const quotient{ratio.numerator / ratio.denominator};
            if (quotient >= std::numeric_limits<std::int32_t>::min() &&
                quotient <= std::numeric_limits<std::int32_t>::max()) {
                return static_cast<std::int32_t>(quotient);
            }
        }
    }
    error(DiagnosticCode::InvalidNumber, node_path, "Expected a 32-bit integer");
    return 0;
}
auto AstParser::float_number(s7::NodeIndex const node, std::string_view const node_path) -> float {
    auto const value{number(node, node_path)};
    if (std::isfinite(value) && std::abs(value) > std::numeric_limits<float>::max()) {
        error(DiagnosticCode::InvalidNumber, node_path, "Number is outside the float range");
        return 0.0f;
    }
    return static_cast<float>(value);
}
void AstParser::require(std::span<Field const> const fields,
                        Property const property,
                        std::string_view const node_path) {
    if (std::ranges::find(fields, property, &Field::property) == fields.end()) {
        error(DiagnosticCode::MissingProperty,
              std::format("{}.{}", node_path, to_serialized_string(property)),
              std::format("Missing property ':{}'", to_serialized_string(property)));
    }
}
void AstParser::unknown(Field const& field) {
    error(DiagnosticCode::UnknownProperty, field.node_path, "Property is not valid in this record");
}
void AstParser::error(DiagnosticCode const code,
                      std::string_view const node_path,
                      std::string message) {
    errors_.emplace_back(code, std::string{node_path}, std::move(message));
}
}
