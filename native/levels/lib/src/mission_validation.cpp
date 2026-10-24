#include <ioj/levels/mission_validation.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <unordered_map>
#include <unordered_set>

namespace ioj::levels {
namespace mission_validation_detail {
auto has_cycle(std::vector<std::vector<std::uint32_t>> const& edges) -> bool {
    // Use an iterative traversal so authored depth cannot overflow the C++ stack.
    std::vector<std::uint8_t> colors(edges.size());
    std::vector<std::pair<std::uint32_t, std::size_t>> stack;
    auto const count{edges.size()};
    for (std::uint32_t root{}; root < count; ++root) {
        if (colors[root] != 0) {
            continue;
        }
        colors[root] = 1;
        stack.emplace_back(root, 0);
        while (!stack.empty()) {
            auto& [node, next]{stack.back()};
            if (next == edges[node].size()) {
                colors[node] = 2;
                stack.pop_back();
                continue;
            }
            auto const child{edges[node][next++]};
            if (colors[child] == 1) {
                return true;
            }
            if (colors[child] == 0) {
                colors[child] = 1;
                stack.emplace_back(child, 0);
            }
        }
    }
    return false;
}
}

auto validate_mission(MissionDefinition const& mission, std::span<EntityId const> const entities)
    -> std::vector<std::string> {
    constexpr std::uint32_t mission_index_limit{1u << 24};

    std::vector<std::string> errors;
    std::unordered_map<ObjectiveName, std::uint32_t> objectives;
    std::unordered_set<EntityId> declared{entities.begin(), entities.end()};
    auto const count{mission.objectives.size()};
    if (count >= mission_index_limit) {
        errors.emplace_back("Too many objectives");
        return errors;
    }
    for (std::uint32_t index{}; index < count; ++index) {
        auto const& objective{mission.objectives[index]};
        if (objective.id.empty() || !objectives.emplace(objective.id, index).second) {
            errors.emplace_back("Objective IDs must be nonempty and unique: " + objective.id.value);
        }
    }
    auto valid_team = [&](Team const team) {
        if (!std::ranges::contains(EnumTraits<Team>::values, team)) {
            errors.emplace_back("Invalid objective or result team");
        }
    };
    auto reference = [&](ObjectiveDependency const& dependency) -> bool {
        auto const valid{
            objectives.contains(dependency.objective) &&
            std::ranges::contains(EnumTraits<ObjectiveOutcome>::values, dependency.outcome)};
        if (!valid) {
            errors.emplace_back("Invalid objective outcome reference: " +
                                dependency.objective.value);
        }
        return valid;
    };
    std::vector<std::vector<std::uint32_t>> dependencies(count);
    std::uint64_t expression_count{};
    for (std::uint32_t index{}; index < count; ++index) {
        auto const& objective{mission.objectives[index]};
        auto const prefix{std::format("Objective '{}': ", objective.id.value)};
        valid_team(objective.team);
        expression_count += objective.expressions.size();
        if (expression_count >= mission_index_limit) {
            errors.emplace_back("Too many mission expressions for packed rule indices");
            return errors;
        }
        auto has_root{false};
        for (auto const outcome : EnumTraits<ObjectiveOutcome>::values) {
            auto const root{objective.roots[outcome]};
            if (root != absent_expression) {
                has_root = true;
                if (root >= objective.expressions.size()) {
                    errors.emplace_back(prefix + "Invalid expression root");
                }
            }
        }
        if (!has_root) {
            errors.emplace_back(prefix + "At least one outcome root is required");
        }
        std::unordered_set<ObjectiveName> prerequisites;
        for (auto const& dependency : objective.dependencies) {
            if (reference(dependency)) {
                dependencies[index].push_back(objectives.at(dependency.objective));
            }
            if (!prerequisites.insert(dependency.objective).second) {
                errors.emplace_back(prefix + "Duplicate or conflicting activation prerequisite");
            }
        }
        std::vector<std::vector<std::uint32_t>> edges(objective.expressions.size());
        auto const expression_size{objective.expressions.size()};
        for (std::uint32_t node{}; node < expression_size; ++node) {
            auto const& expression{objective.expressions[node]};
            for (auto const child : expression.children()) {
                if (child >= expression_size) {
                    errors.emplace_back(prefix + "Invalid local expression child");
                } else {
                    edges[node].push_back(child);
                }
            }
            auto entity = [&](EntityId const& id) {
                if (!declared.contains(id)) {
                    errors.emplace_back(prefix + "Undeclared entity: " + id.value);
                }
            };
            switch (expression.type()) {
                case ExprType::All:
                case ExprType::Any:
                    break;
                case ExprType::EntityDestroyed: {
                    auto const& condition{std::get<EntityDestroyedExpr>(expression.payload)};
                    entity(condition.entity);
                    break;
                }
                case ExprType::EntityReachedPosition: {
                    auto const& condition{std::get<EntityReachedPositionExpr>(expression.payload)};
                    entity(condition.entity);
                    if (!std::isfinite(condition.radius.get() * condition.radius.get())) {
                        errors.emplace_back(prefix + "Squared radius exceeds the finite range");
                    }
                    break;
                }
                case ExprType::TimeElapsed:
                    break;
                case ExprType::ObjectiveResolved: {
                    auto const& condition{std::get<ObjectiveResolvedExpr>(expression.payload)};
                    reference(condition.objective);
                    break;
                }
                case ExprType::EntityKillsReached: {
                    auto const& condition{std::get<EntityKillsReachedExpr>(expression.payload)};
                    std::unordered_set<EntityId> sources;
                    for (auto const& id : condition.entities) {
                        entity(id);
                        if (!sources.insert(id).second) {
                            errors.emplace_back(prefix + "Duplicate kill source");
                        }
                    }
                    if (condition.entities.empty()) {
                        errors.emplace_back(prefix + "Kill condition requires source entities");
                    }
                    break;
                }
            }
        }
        if (mission_validation_detail::has_cycle(edges)) {
            errors.emplace_back(prefix + "Expression cycle");
        }
    }
    if (mission_validation_detail::has_cycle(dependencies)) {
        errors.emplace_back("Objective activation cycle");
    }
    for (auto const& mapping : mission.team_results) {
        valid_team(mapping.team);
        reference(mapping.when);
        if (mapping.result != TeamOutcome::Victory && mapping.result != TeamOutcome::Defeat) {
            errors.emplace_back("Invalid team result");
        }
    }
    return errors;
}
}
