#include <ioj/sim/missions/compiled_mission.h>

#include <ioj/levels/mission_validation.h>
#include <ioj/sim/sim_clock.h>

#include <limits>
#include <unordered_map>
#include <utility>

namespace ioj::sim {
namespace compiled_mission_detail {
auto expression_order(std::span<levels::MissionExpr const> const expressions)
    -> std::vector<levels::ExprIndex> {
    auto const count{expressions.size()};
    std::vector<levels::ExprIndex> order;
    order.reserve(count);
    std::vector<bool> visited(count);
    std::vector<std::pair<levels::ExprIndex, std::size_t>> stack;

    for (levels::ExprIndex index{}; index < count; ++index) {
        if (visited[index]) {
            continue;
        }
        visited[index] = true;
        stack.emplace_back(index, 0);

        while (!stack.empty()) {
            auto& [node, next_child]{stack.back()};
            auto const children{expressions[node].children()};
            if (next_child == children.size()) {
                order.push_back(node);
                stack.pop_back();
            } else {
                auto const child{children[next_child++]};
                if (!visited[child]) {
                    visited[child] = true;
                    stack.emplace_back(child, 0);
                }
            }
        }
    }
    return order;
}
}

auto CompiledMission::compile(levels::MissionDefinition const& definition,
                              std::span<levels::EntityId const> const entities,
                              SimClock const& clock)
    -> std::expected<CompiledMission, std::vector<std::string>> {
    auto errors{levels::validate_mission(definition, entities)};
    if (!errors.empty()) {
        return std::unexpected{std::move(errors)};
    }
    auto const count{definition.objectives.size()};
    CompiledMission result{.objectives = Objectives{count}};
    std::unordered_map<levels::ObjectiveName, ObjectiveId> objectives;
    std::unordered_map<levels::EntityId, std::uint32_t> entity_indices;
    auto const entity_count{entities.size()};
    for (std::uint32_t index{}; index < entity_count; ++index) {
        entity_indices.emplace(entities[index], index);
    }
    for (std::uint32_t index{}; index < count; ++index) {
        objectives.emplace(definition.objectives[index].id, ObjectiveId{index});
    }
    auto& composites{result.composites};
    auto& leaves{result.leaves};
    auto& rules{result.rules};
    std::vector<std::vector<ActivationNotification>> dependents(count);
    auto size = [](auto const& values) { return static_cast<std::uint32_t>(values.size()); };

    for (std::uint32_t row{}; row < count; ++row) {
        ObjectiveId const owner{row};
        auto const& source{definition.objectives[row]};
        result.objectives.name(owner) = source.id;
        result.objectives.title(owner) = source.title.empty() ? source.id.value : source.title;
        result.objectives.team(owner) = source.team;
        result.objectives.wins_level(owner) = source.wins_level;
        result.objectives.prerequisites(owner) =
            static_cast<ObjectivePrerequisiteCount>(source.dependencies.size());
        result.objectives.leaves(owner).offset = size(leaves.results);
        result.objectives.composites(owner).offset = size(composites.results);
        for (auto const& dependency : source.dependencies) {
            dependents[objectives.at(dependency.objective).index()].push_back(
                {owner, dependency.outcome});
        }
        std::vector<ExprHandle> references(source.expressions.size());
        auto const order{compiled_mission_detail::expression_order(source.expressions)};
        for (auto const index : order) {
            auto const& expression{source.expressions[index]};
            if (expression.type() == ExprType::All || expression.type() == ExprType::Any) {
                ExprCompositeIndex const composite{size(composites.results)};
                references[index] = ExprHandle{composite, false};
                composites.results.push_back(ExprResult::Pending);
                composites.types.push_back(expression.type());
                auto const children{expression.children()};
                composites.child_ranges.push_back({size(result.children), size(children)});
                for (auto const child : children) {
                    result.children.push_back(references[child]);
                }
                continue;
            }
            ExprLeafHandle const leaf{size(leaves.results)};
            references[index] = ExprHandle{leaf, true};
            leaves.results.push_back(ExprResult::Pending);
            leaves.owners.push_back(owner);
            RuleType type{};
            switch (expression.type()) {
                case ExprType::EntityDestroyed: {
                    auto const& condition{
                        std::get<levels::EntityDestroyedExpr>(expression.payload)};
                    type = RuleType::EntityDestroyed;
                    rules.destroyed_entities.push_back(entity_indices.at(condition.entity));
                    break;
                }
                case ExprType::EntityReachedPosition: {
                    auto const& condition{
                        std::get<levels::EntityReachedPositionExpr>(expression.payload)};
                    type = RuleType::EntityReachedPosition;
                    rules.reached_entities.push_back(entity_indices.at(condition.entity));
                    rules.positions.push_back(condition.position.get());
                    rules.radius_squared.push_back(condition.radius.get() * condition.radius.get());
                    break;
                }
                case ExprType::TimeElapsed: {
                    auto const& condition{std::get<levels::TimeElapsedExpr>(expression.payload)};
                    type = RuleType::TimeElapsed;
                    auto const ticks{condition.seconds.get() * clock.get_tick_rate().get_hz()};
                    if (ticks >=
                        static_cast<double>(std::numeric_limits<SimTick::value_type>::max())) {
                        return std::unexpected{
                            std::vector<std::string>{"Mission timer exceeds tick range"}};
                    }
                    rules.durations.emplace_back(
                        clock.duration_to_tick_period(Duration::make(condition.seconds.get()))
                            .get());
                    break;
                }
                case ExprType::ObjectiveResolved: {
                    auto const& condition{
                        std::get<levels::ObjectiveResolvedExpr>(expression.payload)};
                    type = RuleType::ObjectiveResolved;
                    auto const observed{objectives.at(condition.objective.objective)};
                    rules.observed_objectives.push_back(observed);
                    rules.observed_outcomes.push_back(condition.objective.outcome);
                    break;
                }
                case ExprType::EntityKillsReached: {
                    auto const& condition{
                        std::get<levels::EntityKillsReachedExpr>(expression.payload)};
                    type = RuleType::EntityKillsReached;
                    rules.kill_sources.push_back(
                        {size(rules.kill_entities), size(condition.entities)});
                    rules.kill_targets.push_back(condition.kills);
                    for (auto const& entity : condition.entities) {
                        rules.kill_entities.push_back(entity_indices.at(entity));
                    }
                    break;
                }
                default:
                    std::unreachable();
            }
            auto& rule_leaves{rules.leaves[type]};
            leaves.rules.push_back(RuleId{size(rule_leaves), type});
            rule_leaves.push_back(leaf);
        }
        for (auto const root : EnumTraits<ObjectiveOutcome>::values) {
            if (source.roots[root] != levels::absent_expression) {
                result.objectives.roots(owner)[root] = references[source.roots[root]];
            }
        }
        result.objectives.leaves(owner).count =
            size(leaves.results) - result.objectives.leaves(owner).offset;
        result.objectives.composites(owner).count =
            size(composites.results) - result.objectives.composites(owner).offset;
    }
    for (std::uint32_t row{}; row < count; ++row) {
        ObjectiveId const id{row};
        result.objectives.dependents(id) = {size(result.dependents), size(dependents[row])};
        result.dependents.insert(
            result.dependents.end(), dependents[row].begin(), dependents[row].end());
    }
    return result;
}
}
