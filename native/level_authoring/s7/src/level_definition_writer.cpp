#include <ioj/levels/authoring/level_definition_writer.h>

#include <ioj/enum_traits.h>
#include <ioj/levels/authoring/level_inputs_writer.h>

namespace ioj::levels::authoring {
namespace {

auto expression_input(ObjectiveDefinition const& objective, ExprIndex const index) -> ExprInput {
    ExprInput result{.expression = objective.expressions[index]};
    for (auto const child : result.expression.children()) {
        result.children.push_back(expression_input(objective, child));
    }
    if (auto* all{std::get_if<AllExpr>(&result.expression.payload)}) {
        all->children.clear();
    } else if (auto* any{std::get_if<AnyExpr>(&result.expression.payload)}) {
        any->children.clear();
    }
    return result;
}
}

auto make_entity_input(EntitySpawnDefinition const& entity) -> EntityInput {
    return {.id = entity.id,
            .archetype = entity.archetype,
            .team = entity.team,
            .position = entity.position,
            .rotation = {entity.rotation.pitch, entity.rotation.yaw, entity.rotation.roll},
            .spawn_at = entity.spawn_time_seconds,
            .health = entity.overrides.health,
            .laser_damage = entity.overrides.laser_damage,
            .initial_spawn_delay = entity.overrides.initial_spawn_delay,
            .spawn_cooldown = entity.overrides.spawn_cooldown,
            .initial_fire_point = entity.overrides.initial_fire_point};
}

auto make_level_input(LevelDefinition const& level) -> LevelInput {
    auto const& metadata{level.metadata()};
    LevelInput result{.id = metadata.id,
                      .title = metadata.title,
                      .description = metadata.description,
                      .par_time = metadata.par_time_seconds,
                      .player = level.player_entity_id(),
                      .camera = level.camera(),
                      .settings = level.settings()};
    if (!level.unlock_level_ids().empty()) {
        result.unlock.emplace();
        for (auto const& id : level.unlock_level_ids()) {
            result.unlock->push_back({id});
        }
    }
    if (level.mission()) {
        result.mission.emplace();
        for (auto const& objective : level.mission()->objectives) {
            ObjectiveInput input{.id = objective.id,
                                 .title = objective.title,
                                 .team = objective.team,
                                 .wins_level = objective.wins_level,
                                 .depends_on = objective.dependencies};
            auto const root{[&](ObjectiveOutcome const outcome) -> std::optional<ExprInput> {
                auto const index{objective.roots[outcome]};
                return index == absent_expression
                         ? std::nullopt
                         : std::optional{expression_input(objective, index)};
            }};
            input.pass = root(ObjectiveOutcome::Passed);
            input.fail = root(ObjectiveOutcome::Failed);
            input.expire = root(ObjectiveOutcome::Expired);
            result.mission->objectives.push_back(std::move(input));
        }
    }
    for (auto const& entity : level.entities()) {
        result.entities.push_back(make_entity_input(entity));
    }
    return result;
}

auto emit_level_source(LevelInput const& level) -> std::string {
    return s7::serialize(level) + "\n";
}
}

namespace ioj::s7 {
auto TypeWriter<levels::authoring::ExprInput>::equivalent(levels::authoring::ExprInput const& value,
                                                          levels::authoring::ExprInput const& base)
    -> bool {
    if (value.reference || base.reference) {
        return value.reference == base.reference;
    }
    auto const type{value.expression.type()};
    if (type != base.expression.type()) {
        return false;
    }
    if (type == levels::ExprType::All || type == levels::ExprType::Any) {
        return s7::equivalent(value.children, base.children);
    }
    return value.expression == base.expression;
}

auto TypeWriter<levels::authoring::ExprInput>::write(SExprIR& ir,
                                                     levels::authoring::ExprInput const& value,
                                                     levels::authoring::ExprInput const*) -> Expr {
    using namespace levels;
    if (value.reference) {
        return ir.form("expr-ref", {s7::write(ir, *value.reference)}).build();
    }
    auto form{ir.form(to_serialized_string(value.expression.type()))};
    std::visit(
        [&](auto const& expression) {
            using T = std::decay_t<decltype(expression)>;
            if constexpr (std::same_as<T, AllExpr> || std::same_as<T, AnyExpr>) {
                for (auto const& child : value.children) {
                    form.add(s7::write(ir, child));
                }
            } else if constexpr (std::same_as<T, EntityDestroyedExpr>) {
                form.add(s7::write(ir, expression.entity));
            } else if constexpr (std::same_as<T, EntityReachedPositionExpr>) {
                form.add(s7::write(ir, expression.entity));
                form.add(s7::write(ir, expression.position));
                form.add(s7::write(ir, expression.radius));
            } else if constexpr (std::same_as<T, TimeElapsedExpr>) {
                form.add(s7::write(ir, expression.seconds));
            } else if constexpr (std::same_as<T, ObjectiveResolvedExpr>) {
                form.add(s7::write(ir, expression.objective.objective));
                form.add(s7::write(ir, expression.objective.outcome));
            } else if constexpr (std::same_as<T, EntityKillsReachedExpr>) {
                form.add(s7::write(ir, expression.entities));
                form.add(s7::write(ir, expression.kills));
            }
        },
        value.expression.payload);
    return form.build();
}
}
