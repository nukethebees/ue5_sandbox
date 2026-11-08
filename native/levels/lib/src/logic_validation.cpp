#include <ioj/levels/logic_validation.h>

#include <algorithm>
#include <concepts>
#include <format>
#include <type_traits>
#include <unordered_set>
namespace ioj::levels {
auto validate_logic(LogicDefinition const& logic, std::span<EntitySpawnDefinition const> entities)
    -> Diagnostics {
    Diagnostics errors;
    auto error{[&](std::string path, std::string message) {
        errors.emplace_back(
            DiagnosticCode::InvalidLogicStructure, std::move(path), std::move(message));
    }};
    auto entity{[&](EntityId const& id) {
        return std::ranges::find(entities, id, &EntitySpawnDefinition::id);
    }};
    auto require_entity{[&](EntityId const& id, std::string const& path) {
        if (entity(id) == entities.end()) {
            error(path, "Unknown entity: " + id.value);
        }
    }};
    auto const count{logic.expressions.size()};
    auto require_expression{[&](ExprIndex index, std::string const& path) {
        if (index >= count) {
            error(path, std::format("Invalid expression reference: {}", index));
        }
    }};
    auto effects{[&](std::span<Effect const> values, std::string_view parent) {
        auto const effect_count{values.size()};
        for (std::size_t i{}; i < effect_count; ++i) {
            auto const path{std::format("{}[{}]", parent, i)};
            std::visit(
                [&](auto const& effect) {
                    using T = std::decay_t<decltype(effect)>;
                    if constexpr (std::same_as<T, SpawnEffect>) {
                        if (effect.group >= logic.spawn_groups.size()) {
                            error(path + ".group", "Invalid spawn group reference");
                        }
                    } else if constexpr (std::same_as<T, StartTimerEffect>) {
                        if (effect.timer >= logic.timer_count) {
                            error(path + ".timer", "Invalid timer reference");
                        }
                    } else if constexpr (std::same_as<T, WinEffect>) {
                        if (std::ranges::find(EnumTraits<Team>::values, effect.team) ==
                            EnumTraits<Team>::values.end()) {
                            error(path + ".team", "Invalid winning team");
                        }
                    }
                },
                values[i]);
        }
    }};
    effects(logic.initial_effects, "level.logic.initial-effects");
    auto const rule_count{logic.rules.size()};
    for (std::size_t i{}; i < rule_count; ++i) {
        auto const& rule{logic.rules[i]};
        auto const path{std::format("level.logic.rules[{}]", i)};
        require_expression(rule.when, path + ".when");
        effects(rule.effects, path + ".effects");
    }

    auto const latch_count{logic.latches.size()};
    for (std::size_t i{}; i < latch_count; ++i) {
        auto const& latch{logic.latches[i]};
        auto const path{std::format("level.logic.latches[{}]", i)};
        require_expression(latch.when, path + ".when");
        require_expression(latch.value, path + ".value");
    }

    std::unordered_set<std::string> group_names;
    std::unordered_set<EntityId> grouped;
    auto const group_count{logic.spawn_groups.size()};
    for (std::size_t i{}; i < group_count; ++i) {
        auto const& group{logic.spawn_groups[i]};
        auto const path{std::format("level.logic.spawn-groups[{}]", i)};
        if (group.name.empty() || !group_names.insert(group.name).second) {
            error(path + ".name", "Spawn group names must be nonempty and unique");
        }
        if (group.entities.empty()) {
            error(path + ".entities", "Spawn group must contain entities");
        }
        auto const entity_count{group.entities.size()};
        for (std::size_t j{}; j < entity_count; ++j) {
            auto const& id{group.entities[j]};
            auto const location{std::format("{}.entities[{}]", path, j)};
            require_entity(id, location);
            if (!grouped.insert(id).second) {
                error(location, "Entity belongs to more than one spawn group: " + id.value);
            }
            auto const found{entity(id)};
            if (found != entities.end() && (found->archetype == EntityArchetype::PlayerFighter ||
                                            found->spawn_time_seconds != 0.0)) {
                error(location,
                      "Explicit spawn groups cannot contain the player or scheduled entities");
            }
        }
    }
    for (std::size_t i{}; i < count; ++i) {
        auto const& expression{logic.expressions[i]};
        auto const path{std::format("level.logic.expressions[{}]", i)};
        auto const children{expression.children()};
        auto const child_count{children.size()};
        for (std::size_t j{}; j < child_count; ++j) {
            require_expression(children[j], std::format("{}.children[{}]", path, j));
        }
        if (expression.type() == ExprType::Not && child_count != 1) {
            error(path + ".children", "not-of requires one expression");
        }
        std::visit(
            [&](auto const& value) {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::same_as<T, EntityDestroyedExpr> ||
                              std::same_as<T, EntityReachedPositionExpr>) {
                    require_entity(value.entity, path + ".entity");
                } else if constexpr (std::same_as<T, EntityKillsReachedExpr>) {
                    std::unordered_set<EntityId> sources;
                    auto const source_count{value.entities.size()};
                    for (std::size_t j{}; j < source_count; ++j) {
                        auto const& id{value.entities[j]};
                        auto const location{std::format("{}.entities[{}]", path, j)};
                        require_entity(id, location);
                        if (!sources.insert(id).second) {
                            error(location, "Kill sources must be distinct: " + id.value);
                        }
                    }
                } else if constexpr (std::same_as<T, TimerElapsedExpr>) {
                    if (value.timer >= logic.timer_count) {
                        error(path + ".timer", "Invalid timer observation");
                    }
                } else if constexpr (std::same_as<T, LatchedTrueExpr> ||
                                     std::same_as<T, LatchedFalseExpr>) {
                    if (value.latch >= logic.latches.size()) {
                        error(path + ".latch", "Invalid latch observation");
                    }
                }
            },
            expression.payload);
    }
    if (!errors.empty()) {
        return errors;
    }
    struct Visit {
        ExprIndex node;
        std::size_t next{};
    };
    std::vector<std::vector<ExprIndex>> dependencies(count);
    for (std::size_t i{}; i < count; ++i) {
        auto const& expression{logic.expressions[i]};
        dependencies[i].assign(expression.children().begin(), expression.children().end());
        std::visit(
            [&](auto const& value) {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::same_as<T, LatchedTrueExpr> ||
                              std::same_as<T, LatchedFalseExpr>) {
                    auto const& latch{logic.latches[value.latch]};
                    dependencies[i].push_back(latch.when);
                    dependencies[i].push_back(latch.value);
                }
            },
            expression.payload);
    }
    std::vector<std::uint8_t> states(count);
    std::vector<Visit> stack;
    for (ExprIndex index{}; index < count; ++index) {
        if (states[index]) {
            continue;
        }
        states[index] = 1;
        stack.push_back({index});
        while (!stack.empty()) {
            auto& visit{stack.back()};
            auto const& children{dependencies[visit.node]};
            if (visit.next == children.size()) {
                states[visit.node] = 2;
                stack.pop_back();
                continue;
            }
            auto const child{children[visit.next++]};
            if (states[child] == 1) {
                error(
                    std::format("level.logic.expressions[{}]", visit.node),
                    std::format("Logic cycle to expressions[{}]: combinational cycles and authored "
                                "latch feedback are not supported",
                                child));
                return errors;
            }
            if (!states[child]) {
                states[child] = 1;
                stack.push_back({child});
            }
        }
    }
    return errors;
}
}
