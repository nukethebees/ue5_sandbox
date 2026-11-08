#include <ioj/levels/authoring/input_readers.h>

#include <ioj/levels/authoring/grammar.h>

#include <array>
#include <concepts>
#include <type_traits>
namespace ioj::s7 {
namespace logic_reading {
using namespace levels;
auto expression(Reader const& reader, NodeIndex node, std::string_view path)
    -> ReadResult<Expression> {
    auto values{reader.list(node, path)};
    if (!values) {
        return std::unexpected{values.error()};
    }
    auto const& ast{reader.ast()};
    if (values->empty() || ast.node(values->front()).kind != NodeKind::Keyword) {
        return read_error(AstErrorCode::ExpectedRecord, path, "Expected an expression record");
    }
    auto type{try_parse_serialized_expr_type(ast.text(values->front()))};
    if (!type) {
        return read_error(AstErrorCode::UnknownSymbol, path, "Unknown expression constructor");
    }
    if (is_composite(*type)) {
        std::vector<ExprIndex> children;
        for (auto child : values->subspan(1)) {
            auto index{s7::read<ExprIndex>(reader, child, path)};
            if (!index) {
                return std::unexpected{index.error()};
            }
            children.push_back(*index);
        }
        switch (*type) {
            case ExprType::All:
                return Expression{AllExpr{std::move(children)}};
            case ExprType::Any:
                return Expression{AnyExpr{std::move(children)}};
            case ExprType::Not:
                return Expression{NotExpr{std::move(children)}};
            case ExprType::Xor:
                return Expression{XorExpr{std::move(children)}};
            default:
                std::unreachable();
        }
    }
    auto const count{*type == ExprType::EntityReachedPosition
                         ? 3u
                         : (*type == ExprType::EntityKillsReached ? 2u : 1u)};
    auto args{positional_arguments(reader, node, ast.text(values->front()), path, count)};
    if (!args) {
        return std::unexpected{args.error()};
    }
    AstDiagnostics errors;
    auto field{[&]<typename T>(std::size_t index) {
        T value{};
        decode_field(
            {reader, (*args)[index], std::format("{}[{}]", path, index), ReadMode::Create, errors},
            value);
        return value;
    }};
    Expression result;
    switch (*type) {
        case ExprType::EntityDestroyed:
            result = {EntityDestroyedExpr{field.template operator()<EntityId>(0)}};
            break;
        case ExprType::EntityReachedPosition:
            result = {
                EntityReachedPositionExpr{field.template operator()<EntityId>(0),
                                          field.template operator()<FiniteVector<double, 3>>(1),
                                          field.template operator()<NonNegativeFinite<double>>(2)}};
            break;
        case ExprType::EntityKillsReached:
            result = {EntityKillsReachedExpr{field.template operator()<std::vector<EntityId>>(0),
                                             field.template operator()<std::uint32_t>(1)}};
            break;
        case ExprType::TimerElapsed:
            result = {TimerElapsedExpr{field.template operator()<std::uint32_t>(0)}};
            break;
        case ExprType::LatchedTrue:
            result = {LatchedTrueExpr{field.template operator()<std::uint32_t>(0)}};
            break;
        case ExprType::LatchedFalse:
            result = {LatchedFalseExpr{field.template operator()<std::uint32_t>(0)}};
            break;
        case ExprType::EventFired: {
            auto name{reader.symbol((*args)[0], path)};
            if (!name) {
                return std::unexpected{name.error()};
            }
            result = {EventFiredExpr{std::move(*name)}};
            break;
        }
        default:
            std::unreachable();
    }
    if (!errors.empty()) {
        return std::unexpected{std::move(errors)};
    }
    return result;
}
auto effect(Reader const& reader, NodeIndex node, std::string_view path) -> ReadResult<Effect> {
    auto values{reader.list(node, path)};
    if (!values) {
        return std::unexpected{values.error()};
    }
    auto const& ast{reader.ast()};
    if (values->empty() || ast.node(values->front()).kind != NodeKind::Keyword) {
        return read_error(AstErrorCode::ExpectedRecord, path, "Expected an effect");
    }
    using authoring::EffectSyntax;
    auto type{authoring::try_parse_serialized_effect_syntax(ast.text(values->front()))};
    if (!type) {
        return read_error(AstErrorCode::UnknownSymbol, path, "Unknown effect");
    }
    auto const count{*type == EffectSyntax::Stop ? 0u
                                                 : (*type == EffectSyntax::StartTimer ? 2u : 1u)};
    auto args{positional_arguments(reader, node, ast.text(values->front()), path, count)};
    if (!args) {
        return std::unexpected{args.error()};
    }
    switch (*type) {
        case EffectSyntax::Stop:
            return StopEffect{};
        case EffectSyntax::Win:
            return s7::read<Team>(reader, (*args)[0], path).transform([](auto v) -> Effect {
                return WinEffect{v};
            });
        case EffectSyntax::Spawn:
            return s7::read<std::uint32_t>(reader, (*args)[0], path)
                .transform([](auto v) -> Effect { return SpawnEffect{v}; });
        case EffectSyntax::Publish:
            return reader.symbol((*args)[0], path).transform([](auto v) -> Effect {
                return PublishEffect{std::move(v)};
            });
        case EffectSyntax::StartTimer: {
            auto timer{s7::read<std::uint32_t>(reader, (*args)[0], path)};
            auto duration{s7::read<NonNegativeFinite<double>>(reader, (*args)[1], path)};
            if (!timer) {
                return std::unexpected{timer.error()};
            }
            if (!duration) {
                return std::unexpected{duration.error()};
            }
            return StartTimerEffect{*timer, *duration};
        }
    }
    std::unreachable();
}
template <typename T, typename Read>
auto sequence(Reader const& reader, NodeIndex node, std::string_view path, Read read)
    -> ReadResult<std::vector<T>> {
    auto values{reader.list(node, path)};
    if (!values) {
        return std::unexpected{values.error()};
    }
    std::vector<T> result;
    for (auto item : *values) {
        auto value{read(reader, item, path)};
        if (!value) {
            return std::unexpected{value.error()};
        }
        result.push_back(std::move(*value));
    }
    return result;
}
auto group(Reader const& reader, NodeIndex node, std::string_view path)
    -> ReadResult<SpawnGroupDefinition> {
    auto fields{s7::read<std::array<RawNode, 2>>(reader, node, path)};
    if (!fields) {
        return std::unexpected{fields.error()};
    }
    auto name{reader.symbol((*fields)[0].index, path)};
    auto entities{s7::read<std::vector<EntityId>>(reader, (*fields)[1].index, path)};
    if (!name) {
        return std::unexpected{name.error()};
    }
    if (!entities) {
        return std::unexpected{entities.error()};
    }
    return SpawnGroupDefinition{std::move(*name), std::move(*entities)};
}
auto rule(Reader const& reader, NodeIndex node, std::string_view path) -> ReadResult<EventRule> {
    auto fields{s7::read<std::array<RawNode, 2>>(reader, node, path)};
    if (!fields) {
        return std::unexpected{fields.error()};
    }
    auto when{s7::read<ExprIndex>(reader, (*fields)[0].index, path)};
    auto effects{sequence<Effect>(reader, (*fields)[1].index, path, effect)};
    if (!when) {
        return std::unexpected{when.error()};
    }
    if (!effects) {
        return std::unexpected{effects.error()};
    }
    return EventRule{*when, std::move(*effects)};
}
}
auto TypeReader<levels::LogicDefinition>::read(Reader const& reader,
                                               NodeIndex node,
                                               std::string_view path)
    -> ReadResult<levels::LogicDefinition> {
    using namespace levels;
    auto args{positional_arguments(reader, node, "logic", path, 6)};
    if (!args) {
        return std::unexpected{args.error()};
    }
    auto expressions{
        logic_reading::sequence<Expression>(reader, (*args)[0], path, logic_reading::expression)};
    auto latches{s7::read<std::vector<std::array<ExprIndex, 2>>>(reader, (*args)[1], path)};
    auto timers{s7::read<std::uint32_t>(reader, (*args)[2], path)};
    auto groups{logic_reading::sequence<SpawnGroupDefinition>(
        reader, (*args)[3], path, logic_reading::group)};
    auto initial{logic_reading::sequence<Effect>(reader, (*args)[4], path, logic_reading::effect)};
    auto rules{logic_reading::sequence<EventRule>(reader, (*args)[5], path, logic_reading::rule)};
    if (!expressions) {
        return std::unexpected{expressions.error()};
    }
    if (!latches) {
        return std::unexpected{latches.error()};
    }
    if (!timers) {
        return std::unexpected{timers.error()};
    }
    if (!groups) {
        return std::unexpected{groups.error()};
    }
    if (!initial) {
        return std::unexpected{initial.error()};
    }
    if (!rules) {
        return std::unexpected{rules.error()};
    }
    LogicDefinition result{.expressions = std::move(*expressions),
                           .timer_count = *timers,
                           .spawn_groups = std::move(*groups),
                           .initial_effects = std::move(*initial),
                           .rules = std::move(*rules)};
    for (auto const& latch : *latches) {
        result.latches.push_back({latch[0], latch[1]});
    }
    return result;
}
}
