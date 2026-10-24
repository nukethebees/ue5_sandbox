#include <ioj/levels/authoring/input_readers.h>

namespace ioj::s7 {
namespace {
using namespace levels;
using namespace levels::authoring;

auto arguments(Reader const& reader,
               NodeIndex node,
               std::string_view path,
               std::string_view tag,
               std::optional<std::size_t> count = {}) -> ReadResult<std::span<NodeIndex const>> {
    auto values{reader.list(node, path)};
    if (!values) {
        return values;
    }
    auto const& ast{reader.ast()};
    if (values->empty() || ast.node(values->front()).kind != NodeKind::Keyword ||
        ast.text(values->front()) != tag) {
        return read_error(AstErrorCode::ExpectedRecord, path, std::format("Expected '{}'", tag));
    }
    if (count && values->size() != *count + 1) {
        return read_error(AstErrorCode::InvalidType,
                          path,
                          std::format("'{}' requires {} arguments", tag, *count));
    }
    return values->subspan(1);
}

auto read_expr(Reader const& reader, NodeIndex node, std::string_view path, std::size_t depth)
    -> ReadResult<ExprInput> {
    if (depth > 256) {
        return read_error(AstErrorCode::LimitExceeded, path, "Expression nesting exceeds 256");
    }
    auto const values{reader.list(node, path)};
    if (!values) {
        return std::unexpected{values.error()};
    }
    auto const& ast{reader.ast()};
    if (values->empty() || ast.node(values->front()).kind != NodeKind::Keyword) {
        return read_error(AstErrorCode::ExpectedRecord, path, "Expected a mission expression");
    }
    auto const tag{ast.text(values->front())};
    if (tag == "expr-ref") {
        return arguments(reader, node, path, tag, 1).and_then([&](auto args) {
            return s7::read<ObjectiveName>(reader, args[0], path).transform([](auto name) {
                return ExprInput{.reference = std::move(name)};
            });
        });
    }
    auto const type{try_parse_serialized_expr_type(tag)};
    if (!type) {
        return read_error(
            AstErrorCode::UnknownSymbol, path, std::format("Unknown mission expression '{}'", tag));
    }
    if (*type == ExprType::All || *type == ExprType::Any) {
        ExprInput result;
        if (*type == ExprType::Any) {
            result.expression.payload = AnyExpr{};
        }
        AstDiagnostics errors;
        auto const count{values->size()};
        for (std::size_t i{1}; i < count; ++i) {
            auto child{read_expr(reader, (*values)[i], std::format("{}[{}]", path, i), depth + 1)};
            if (child) {
                result.children.push_back(std::move(*child));
            } else {
                append_errors(errors, std::move(child.error()));
            }
        }
        if (!errors.empty()) {
            return std::unexpected{std::move(errors)};
        }
        return result;
    }

    if (*type == ExprType::ObjectiveResolved) {
        return s7::read<ObjectiveDependency>(reader, node, path).transform([](auto dependency) {
            return ExprInput{.expression = {ObjectiveResolvedExpr{std::move(dependency)}}};
        });
    }
    auto const count{*type == ExprType::EntityReachedPosition
                         ? 3u
                         : (*type == ExprType::EntityKillsReached ? 2u : 1u)};
    auto const args{arguments(reader, node, path, tag, count)};
    if (!args) {
        return std::unexpected{args.error()};
    }
    AstDiagnostics errors;
    auto const field{[&]<typename T>(std::size_t index, std::string_view name) {
        std::optional<T> value;
        read_field(reader, (*args)[index], std::format("{}.{}", path, name), value, errors);
        return value;
    }};
    ExprInput result;
    switch (*type) {
        case ExprType::EntityDestroyed:
            if (auto id{field.template operator()<EntityId>(0, "entity")}) {
                result.expression.payload = EntityDestroyedExpr{std::move(*id)};
            }
            break;
        case ExprType::EntityReachedPosition: {
            auto id{field.template operator()<EntityId>(0, "entity")};
            auto position{field.template operator()<FiniteVector<double, 3>>(1, "position")};
            auto radius{field.template operator()<NonNegativeFinite<double>>(2, "radius")};
            if (id && position && radius) {
                result.expression.payload =
                    EntityReachedPositionExpr{std::move(*id), *position, *radius};
            }
            break;
        }
        case ExprType::TimeElapsed:
            if (auto seconds{field.template operator()<NonNegativeFinite<double>>(0, "seconds")}) {
                result.expression.payload = TimeElapsedExpr{*seconds};
            }
            break;
        case ExprType::EntityKillsReached: {
            auto sources{field.template operator()<std::vector<EntityId>>(0, "sources")};
            auto kills{field.template operator()<std::uint32_t>(1, "kills")};
            if (sources && kills) {
                result.expression.payload = EntityKillsReachedExpr{std::move(*sources), *kills};
            }
            break;
        }
        case ExprType::All:
        case ExprType::Any:
        case ExprType::ObjectiveResolved:
            std::unreachable();
    }
    if (!errors.empty()) {
        return std::unexpected{std::move(errors)};
    }
    return result;
}
}

auto TypeReader<levels::authoring::ExprInput>::read(Reader const& reader,
                                                    NodeIndex node,
                                                    std::string_view path)
    -> ReadResult<ExprInput> {
    return read_expr(reader, node, path, 0);
}

auto TypeReader<levels::ObjectiveDependency>::read(Reader const& reader,
                                                   NodeIndex node,
                                                   std::string_view path)
    -> ReadResult<ObjectiveDependency> {
    return arguments(reader, node, path, "objective-resolved", 2)
        .and_then([&](auto args) -> ReadResult<ObjectiveDependency> {
            auto id{s7::read<ObjectiveName>(reader, args[0], std::format("{}.objective", path))};
            auto outcome{
                s7::read<ObjectiveOutcome>(reader, args[1], std::format("{}.outcome", path))};
            if (!id) {
                return std::unexpected{std::move(id.error())};
            }
            if (!outcome) {
                return std::unexpected{std::move(outcome.error())};
            }
            return ObjectiveDependency{std::move(*id), *outcome};
        });
}

auto TypeReader<levels::authoring::NamedExprInput>::read(Reader const& reader,
                                                         NodeIndex node,
                                                         std::string_view path)
    -> ReadResult<NamedExprInput> {
    return arguments(reader, node, path, "expr", 2)
        .and_then([&](auto args) -> ReadResult<NamedExprInput> {
            auto name{s7::read<ObjectiveName>(reader, args[0], std::format("{}.name", path))};
            auto value{s7::read<ExprInput>(reader, args[1], std::format("{}.value", path))};
            if (!name) {
                return std::unexpected{std::move(name.error())};
            }
            if (!value) {
                return std::unexpected{std::move(value.error())};
            }
            return NamedExprInput{std::move(*name), std::move(*value)};
        });
}

auto TypeReader<levels::authoring::UnlockInput>::read(Reader const& reader,
                                                      NodeIndex node,
                                                      std::string_view path)
    -> ReadResult<UnlockInput> {
    return arguments(reader, node, path, "level-completed", 1).and_then([&](auto args) {
        return s7::read<LevelId>(reader, args[0], std::format("{}.level-id", path))
            .transform([](auto id) { return UnlockInput{std::move(id)}; });
    });
}
}
