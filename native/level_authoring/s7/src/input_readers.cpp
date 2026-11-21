#include <ioj/levels/authoring/input_readers.h>

#include <ioj/levels/authoring/grammar.h>
#include <ioj/levels/logic_payloads_reader.h>

#include <array>
#include <format>
namespace ioj::s7 {
using namespace levels;

template <typename Result, typename Payload>
auto read_payload(Reader const& reader, NodeIndex node, std::string_view path)
    -> ReadResult<Result> {
    return s7::read<Payload>(reader, node, path).transform([](Payload value) {
        return Result{std::move(value)};
    });
}

template <>
struct TypeReader<Expression> {
    static auto read(Reader const& reader, NodeIndex node, std::string_view path)
        -> ReadResult<Expression> {
        auto values{reader.list(node, path)};
        if (!values) {
            return ReadResult<Expression>{std::unexpect, std::move(values.error())};
        }
        auto const& ast{reader.ast()};
        if (values->empty() || ast.node(values->front()).kind != NodeKind::Keyword) {
            return read_error(AstErrorCode::ExpectedRecord, path, "Expected an expression record");
        }
        auto type{try_parse_serialized_expr_kind(ast.text(values->front()))};
        if (!type) {
            return read_error(AstErrorCode::UnknownSymbol, path, "Unknown expression constructor");
        }
        if (is_composite(*type)) {
            std::vector<ExprIndex> children;
            for (auto child : values->subspan(1)) {
                auto index{s7::read<ExprIndex>(
                    reader, child, std::format("{}.children[{}]", path, children.size()))};
                if (!index) {
                    return ReadResult<Expression>{std::unexpect, std::move(index.error())};
                }
                children.push_back(*index);
            }
            switch (*type) {
                case ExprKind::All:
                    return ReadResult<Expression>{std::in_place, AllExpr{std::move(children)}};
                case ExprKind::Any:
                    return ReadResult<Expression>{std::in_place, AnyExpr{std::move(children)}};
                case ExprKind::Not:
                    return ReadResult<Expression>{std::in_place, NotExpr{std::move(children)}};
                case ExprKind::Xor:
                    return ReadResult<Expression>{std::in_place, XorExpr{std::move(children)}};
                default:
                    std::unreachable();
            }
        }
        switch (*type) {
            case ExprKind::EntityDestroyed:
                return read_payload<Expression, EntityDestroyedExpr>(reader, node, path);
            case ExprKind::EntityReachedPosition:
                return read_payload<Expression, EntityReachedPositionExpr>(reader, node, path);
            case ExprKind::EntityKillsReached:
                return read_payload<Expression, EntityKillsReachedExpr>(reader, node, path);
            case ExprKind::TimerElapsed:
                return read_payload<Expression, TimerElapsedExpr>(reader, node, path);
            case ExprKind::LatchedTrue:
                return read_payload<Expression, LatchedTrueExpr>(reader, node, path);
            case ExprKind::LatchedFalse:
                return read_payload<Expression, LatchedFalseExpr>(reader, node, path);
            default:
                std::unreachable();
        }
    }
};
template <>
struct TypeReader<Effect> {
    static auto read(Reader const& reader, NodeIndex node, std::string_view path)
        -> ReadResult<Effect> {
        auto values{reader.list(node, path)};
        if (!values) {
            return ReadResult<Effect>{std::unexpect, std::move(values.error())};
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
        switch (*type) {
            case EffectSyntax::Win:
                return read_payload<Effect, WinEffect>(reader, node, path);
            case EffectSyntax::Stop:
                return read_payload<Effect, StopEffect>(reader, node, path);
            case EffectSyntax::Spawn:
                return read_payload<Effect, SpawnEffect>(reader, node, path);
            case EffectSyntax::StartTimer:
                return read_payload<Effect, StartTimerEffect>(reader, node, path);
        }
        std::unreachable();
    }
};
template <>
struct TypeReader<SpawnGroupDefinition> {
    static auto read(Reader const& reader, NodeIndex node, std::string_view path)
        -> ReadResult<SpawnGroupDefinition> {
        auto fields{s7::read<std::array<RawNode, 2>>(reader, node, path)};
        if (!fields) {
            return ReadResult<SpawnGroupDefinition>{std::unexpect, std::move(fields.error())};
        }
        auto name{reader.symbol((*fields)[0].index, field_path(path, "name"))};
        auto entities{s7::read<std::vector<EntityId>>(
            reader, (*fields)[1].index, field_path(path, "entities"))};
        if (!name) {
            return ReadResult<SpawnGroupDefinition>{std::unexpect, std::move(name.error())};
        }
        if (!entities) {
            return ReadResult<SpawnGroupDefinition>{std::unexpect, std::move(entities.error())};
        }
        return ReadResult<SpawnGroupDefinition>{
            std::in_place, std::move(*name), std::move(*entities)};
    }
};
template <>
struct TypeReader<EventRule> {
    static auto read(Reader const& reader, NodeIndex node, std::string_view path)
        -> ReadResult<EventRule> {
        auto fields{s7::read<std::array<RawNode, 2>>(reader, node, path)};
        if (!fields) {
            return ReadResult<EventRule>{std::unexpect, std::move(fields.error())};
        }
        auto when{s7::read<ExprIndex>(reader, (*fields)[0].index, field_path(path, "when"))};
        auto effects{
            s7::read<std::vector<Effect>>(reader, (*fields)[1].index, field_path(path, "effects"))};
        if (!when) {
            return ReadResult<EventRule>{std::unexpect, std::move(when.error())};
        }
        if (!effects) {
            return ReadResult<EventRule>{std::unexpect, std::move(effects.error())};
        }
        return ReadResult<EventRule>{std::in_place, *when, std::move(*effects)};
    }
};

template <>
struct TypeReader<LatchDefinition> {
    static auto read(Reader const& reader, NodeIndex node, std::string_view path)
        -> ReadResult<LatchDefinition> {
        auto fields{s7::read<std::array<RawNode, 2>>(reader, node, path)};
        if (!fields) {
            return ReadResult<LatchDefinition>{std::unexpect, std::move(fields.error())};
        }
        auto when{s7::read<ExprIndex>(reader, (*fields)[0].index, field_path(path, "when"))};
        auto value{s7::read<ExprIndex>(reader, (*fields)[1].index, field_path(path, "value"))};
        if (!when) {
            return ReadResult<LatchDefinition>{std::unexpect, std::move(when.error())};
        }
        if (!value) {
            return ReadResult<LatchDefinition>{std::unexpect, std::move(value.error())};
        }
        return ReadResult<LatchDefinition>{std::in_place, *when, *value};
    }
};
auto TypeReader<levels::LogicDefinition>::read(Reader const& reader,
                                               NodeIndex node,
                                               std::string_view path)
    -> ReadResult<levels::LogicDefinition> {
    using namespace levels;
    auto args{positional_arguments(reader, node, "logic", path, 6)};
    if (!args) {
        return ReadResult<levels::LogicDefinition>{std::unexpect, std::move(args.error())};
    }
    auto expressions{
        s7::read<std::vector<Expression>>(reader, (*args)[0], field_path(path, "expressions"))};
    auto latches{
        s7::read<std::vector<LatchDefinition>>(reader, (*args)[1], field_path(path, "latches"))};
    auto timers{s7::read<std::uint32_t>(reader, (*args)[2], field_path(path, "timer-count"))};
    auto groups{s7::read<std::vector<SpawnGroupDefinition>>(
        reader, (*args)[3], field_path(path, "spawn-groups"))};
    auto initial{
        s7::read<std::vector<Effect>>(reader, (*args)[4], field_path(path, "initial-effects"))};
    auto rules{s7::read<std::vector<EventRule>>(reader, (*args)[5], field_path(path, "rules"))};
    if (!expressions) {
        return ReadResult<levels::LogicDefinition>{std::unexpect, std::move(expressions.error())};
    }
    if (!latches) {
        return ReadResult<levels::LogicDefinition>{std::unexpect, std::move(latches.error())};
    }
    if (!timers) {
        return ReadResult<levels::LogicDefinition>{std::unexpect, std::move(timers.error())};
    }
    if (!groups) {
        return ReadResult<levels::LogicDefinition>{std::unexpect, std::move(groups.error())};
    }
    if (!initial) {
        return ReadResult<levels::LogicDefinition>{std::unexpect, std::move(initial.error())};
    }
    if (!rules) {
        return ReadResult<levels::LogicDefinition>{std::unexpect, std::move(rules.error())};
    }
    return LogicDefinition{.expressions = std::move(*expressions),
                           .latches = std::move(*latches),
                           .timer_count = *timers,
                           .spawn_groups = std::move(*groups),
                           .initial_effects = std::move(*initial),
                           .rules = std::move(*rules)};
}
}
