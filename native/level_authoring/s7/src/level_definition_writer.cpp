#include <ioj/levels/authoring/level_definition_writer.h>

#include <ioj/enum_traits.h>
#include <ioj/levels/authoring/level_inputs_writer.h>

#include <concepts>
#include <type_traits>

namespace ioj::levels::authoring {
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
    result.logic = level.logic();
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
namespace logic_writing {
using namespace levels;
auto symbol(SExprIR& ir, std::string const& text) -> Expr {
    return ir.form("string->symbol", {ir.string(text)}).build();
}
auto expression(SExprIR& ir, Expression const& value) -> Expr {
    auto form{ir.form(
        "list", {ir.atom(std::string{":"} + std::string{to_serialized_string(value.type())})})};
    std::visit(
        [&](auto const& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (requires { v.children; }) {
                for (auto child : v.children) {
                    form.add(s7::write(ir, child));
                }
            } else if constexpr (std::same_as<T, EntityDestroyedExpr>) {
                form.add(s7::write(ir, v.entity));
            } else if constexpr (std::same_as<T, EntityReachedPositionExpr>) {
                form.add(s7::write(ir, v.entity));
                form.add(s7::write(ir, v.position));
                form.add(s7::write(ir, v.radius));
            } else if constexpr (std::same_as<T, EntityKillsReachedExpr>) {
                form.add(s7::write(ir, v.entities));
                form.add(s7::write(ir, v.kills));
            } else if constexpr (std::same_as<T, TimerElapsedExpr>) {
                form.add(s7::write(ir, v.timer));
            } else {
                form.add(s7::write(ir, v.latch));
            }
        },
        value.payload);
    return form.build();
}
auto effect(SExprIR& ir, Effect const& value) -> Expr {
    return std::visit(
        [&](auto const& v) -> Expr {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::same_as<T, WinEffect>) {
                return ir.form("list", {ir.atom(":win"), s7::write(ir, v.team)}).build();
            } else if constexpr (std::same_as<T, StopEffect>) {
                return ir.form("list", {ir.atom(":stop")}).build();
            } else if constexpr (std::same_as<T, SpawnEffect>) {
                return ir.form("list", {ir.atom(":spawn"), s7::write(ir, v.group)}).build();
            } else {
                return ir
                    .form(
                        "list",
                        {ir.atom(":start-timer"), s7::write(ir, v.timer), s7::write(ir, v.seconds)})
                    .build();
            }
        },
        value);
}
template <typename T, typename Write>
auto sequence(SExprIR& ir, std::vector<T> const& values, Write write) -> Expr {
    auto form{ir.form("list")};
    for (auto const& value : values) {
        form.add(write(ir, value));
    }
    return form.build();
}
auto effects(SExprIR& ir, std::vector<Effect> const& values) -> Expr {
    return sequence(ir, values, effect);
}
}
auto TypeWriter<levels::LogicDefinition>::equivalent(levels::LogicDefinition const& value,
                                                     levels::LogicDefinition const& base) -> bool {
    return value == base;
}
auto TypeWriter<levels::LogicDefinition>::write(SExprIR& ir,
                                                levels::LogicDefinition const& value,
                                                levels::LogicDefinition const*) -> Expr {
    using namespace logic_writing;
    return ir
        .form("list",
              {ir.atom(":logic"),
               sequence(ir, value.expressions, expression),
               sequence(ir,
                        value.latches,
                        [](auto& ir, auto const& v) {
                            return ir.form("list", {s7::write(ir, v.when), s7::write(ir, v.value)})
                                .build();
                        }),
               s7::write(ir, value.timer_count),
               sequence(ir,
                        value.spawn_groups,
                        [](auto& ir, auto const& v) {
                            return ir.form("list", {symbol(ir, v.name), s7::write(ir, v.entities)})
                                .build();
                        }),
               effects(ir, value.initial_effects),
               sequence(ir,
                        value.rules,
                        [](auto& ir, auto const& v) {
                            return ir.form("list", {s7::write(ir, v.when), effects(ir, v.effects)})
                                .build();
                        })})
        .build();
}
}
