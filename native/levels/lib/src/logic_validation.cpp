#include <ioj/levels/logic_validation.h>

#include <algorithm>
#include <concepts>
#include <type_traits>
#include <unordered_set>
namespace ioj::levels {
auto validate_logic(LogicDefinition const& logic, std::span<EntitySpawnDefinition const> entities)
    -> Diagnostics {
    Diagnostics errors;
    auto error{[&](std::string message) {
        errors.emplace_back(
            DiagnosticCode::InvalidLogicStructure, "level.logic", std::move(message));
    }};
    auto entity{[&](EntityId const& id) {
        return std::ranges::find(entities, id, &EntitySpawnDefinition::id);
    }};
    auto require_entity{[&](EntityId const& id) {
        if (entity(id) == entities.end()) {
            error("Unknown entity: " + id.value);
        }
    }};
    auto const count{logic.expressions.size()};
    auto require_expression{[&](ExprIndex index) {
        if (index >= count) {
            error("Invalid expression reference");
        }
    }};
    std::unordered_set<std::string> published;
    auto effects{[&](std::span<Effect const> values) {
        for (auto const& value : values) {
            std::visit(
                [&](auto const& effect) {
                    using T = std::decay_t<decltype(effect)>;
                    if constexpr (std::same_as<T, PublishEffect>) {
                        if (effect.signal.empty()) {
                            error("Signal name must not be empty");
                        }
                        published.insert(effect.signal);
                    } else if constexpr (std::same_as<T, SpawnEffect>) {
                        if (effect.group >= logic.spawn_groups.size()) {
                            error("Invalid spawn group reference");
                        }
                    } else if constexpr (std::same_as<T, StartTimerEffect>) {
                        if (effect.timer >= logic.timer_count) {
                            error("Invalid timer reference");
                        }
                    } else if constexpr (std::same_as<T, WinEffect>) {
                        if (std::ranges::find(EnumTraits<Team>::values, effect.team) ==
                            EnumTraits<Team>::values.end()) {
                            error("Invalid winning team");
                        }
                    }
                },
                value);
        }
    }};
    effects(logic.initial_effects);
    for (auto const& rule : logic.rules) {
        require_expression(rule.when);
        effects(rule.effects);
    }
    for (auto const& latch : logic.latches) {
        require_expression(latch.when);
        require_expression(latch.value);
    }
    std::unordered_set<std::string> group_names;
    std::unordered_set<EntityId> grouped;
    for (auto const& group : logic.spawn_groups) {
        if (group.name.empty() || !group_names.insert(group.name).second) {
            error("Spawn group names must be nonempty and unique");
        }
        if (group.entities.empty()) {
            error("Spawn group must contain entities");
        }
        for (auto const& id : group.entities) {
            require_entity(id);
            if (!grouped.insert(id).second) {
                error("Entity belongs to more than one spawn group: " + id.value);
            }
            auto const found{entity(id)};
            if (found != entities.end() && (found->archetype == EntityArchetype::PlayerFighter ||
                                            found->spawn_time_seconds != 0.0)) {
                error("Explicit spawn groups cannot contain the player or scheduled entities");
            }
        }
    }
    for (auto const& expression : logic.expressions) {
        for (auto const child : expression.children()) {
            require_expression(child);
        }
        if (expression.type() == ExprType::Not && expression.children().size() != 1) {
            error("not-of requires one expression");
        }
        std::visit(
            [&](auto const& value) {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::same_as<T, EntityDestroyedExpr> ||
                              std::same_as<T, EntityReachedPositionExpr>) {
                    require_entity(value.entity);
                } else if constexpr (std::same_as<T, EntityKillsReachedExpr>) {
                    std::unordered_set<EntityId> sources;
                    for (auto const& id : value.entities) {
                        require_entity(id);
                        if (!sources.insert(id).second) {
                            error("Kill sources must be distinct: " + id.value);
                        }
                    }
                } else if constexpr (std::same_as<T, TimerElapsedExpr>) {
                    if (value.timer >= logic.timer_count) {
                        error("Invalid timer observation");
                    }
                } else if constexpr (std::same_as<T, LatchedTrueExpr> ||
                                     std::same_as<T, LatchedFalseExpr>) {
                    if (value.latch >= logic.latches.size()) {
                        error("Invalid latch observation");
                    }
                } else if constexpr (std::same_as<T, EventFiredExpr>) {
                    if (!published.contains(value.signal)) {
                        error("Signal is never published: " + value.signal);
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
                error("Logic cycle: combinational cycles and authored latch feedback are not "
                      "supported");
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
