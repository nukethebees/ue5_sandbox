#include "ast_parser.h"

#include <ioj/constrained_numbers.h>

namespace ioj::levels::authoring::detail {
auto AstParser::record(s7::NodeIndex const node,
                       RecordKind const kind,
                       std::string_view const node_path) -> std::vector<Field> {
    std::vector<Field> fields;
    take(s7::read_record<EnumTraits<Property>::values.size()>(
        s7::Reader{ast_},
        node,
        to_serialized_string(kind),
        node_path,
        try_parse_serialized_property,
        [&](Property property, s7::NodeIndex value, std::string_view path) {
            fields.emplace_back(property, value, std::string{path});
        }));
    return fields;
}
auto AstParser::list(s7::NodeIndex const node, std::string_view const node_path)
    -> std::span<s7::NodeIndex const> {
    return take(s7::Reader{ast_}.list(node, node_path));
}
auto AstParser::symbol(s7::NodeIndex const node, std::string_view const node_path) -> std::string {
    return take(s7::Reader{ast_}.symbol(node, node_path));
}
auto AstParser::nonempty_symbol(s7::NodeIndex const node, std::string_view const node_path)
    -> std::optional<NonEmptyString> {
    auto value{NonEmptyString::try_make(symbol(node, node_path))};
    if (!value && ast_.node(node).kind == s7::NodeKind::Symbol) {
        error(DiagnosticCode::InvalidSymbol, node_path, "Symbol must not be empty");
    }
    return value;
}

auto AstParser::team(s7::NodeIndex const node, std::string_view const node_path) -> Team {
    auto const text{symbol(node, node_path)};
    auto const value{try_parse_serialized_team(text)};
    if (!value && ast_.node(node).kind == s7::NodeKind::Symbol) {
        error(DiagnosticCode::UnsupportedTeam, node_path, std::format("Unknown team '{}'", text));
    }
    return value.value_or(Team::White);
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
auto AstParser::string(s7::NodeIndex const node, std::string_view const node_path) -> std::string {
    return read<std::string>(node, node_path);
}
auto AstParser::number(s7::NodeIndex const node, std::string_view const node_path) -> double {
    return read<double>(node, node_path);
}
auto AstParser::integer(s7::NodeIndex const node, std::string_view const node_path)
    -> std::int32_t {
    return read<std::int32_t>(node, node_path);
}
auto AstParser::float_number(s7::NodeIndex const node, std::string_view const node_path) -> float {
    using FiniteFloat =
        ConstrainedValue<float,
                         ConstrainedValuePolicy{[](float value) { return std::isfinite(value); },
                                                "Expected a finite number"}>;
    return take(s7::read<FiniteFloat>(s7::Reader{ast_}, node, node_path).transform([](auto value) {
        return value.get();
    }));
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
