#include <ioj/levels/mission_validation.h>
#include <ioj/sim/missions/mission.h>

#include <algorithm>
#include <limits>
#include <unordered_map>

namespace ioj::sim {
auto compile_mission(levels::MissionDefinition const& definition,
                     std::span<levels::EntityId const> const entities,
                     SimClock const& clock)
    -> std::expected<CompiledMission, std::vector<std::string>> {
    auto errors{levels::validate_mission(definition, entities)};
    if (!errors.empty()) {
        return std::unexpected{std::move(errors)};
    }
    CompiledMission result;
    std::unordered_map<std::string, ObjectiveId> objectives;
    std::unordered_map<levels::EntityId, std::uint32_t> entity_indices;
    auto const entity_count{entities.size()};
    for (std::uint32_t index{}; index < entity_count; ++index) {
        entity_indices.emplace(entities[index], index);
    }
    auto const count{definition.objectives.size()};
    result.objectives.resize(count);
    for (ObjectiveId index{}; index < count; ++index) {
        objectives.emplace(definition.objectives[index].id, index);
    }
    auto& composites{result.composites};
    auto& leaves{result.leaves};
    auto& rules{result.rules};
    std::vector<std::vector<std::uint32_t>> composite_parents, leaf_parents;
    std::vector<std::vector<ActivationNotification>> dependents(count);
    std::vector<std::vector<std::uint32_t>> observers(count), mappings(count);
    auto size = [](auto const& values) { return static_cast<std::uint32_t>(values.size()); };

    for (ObjectiveId owner{}; owner < count; ++owner) {
        auto const& source{definition.objectives[owner]};
        auto& objective{result.objectives[owner]};
        objective.id = source.id;
        objective.title = source.title.empty() ? source.id : source.title;
        objective.team = source.team;
        objective.prerequisites = size(source.dependencies);
        objective.leaves.offset = size(leaves.results);
        objective.composites.offset = size(composites.results);
        for (auto const& dependency : source.dependencies) {
            dependents[objectives.at(dependency.objective)].push_back({owner, dependency.outcome});
        }
        std::vector<ExprRef> references;
        references.reserve(source.expressions.size());
        for (auto const& expression : source.expressions) {
            if (expression.type == ExpressionType::All || expression.type == ExpressionType::Any) {
                references.push_back({size(composites.results)});
                composites.results.push_back(ExprResult::Pending);
                composites.types.push_back(expression.type);
                composites.owners.push_back(owner);
                composites.parents.emplace_back();
                composites.children.emplace_back();
                composites.remaining.push_back(0);
                composite_parents.emplace_back();
                continue;
            }
            auto const leaf{size(leaves.results)};
            references.push_back({ExprRef::leaf_bit | leaf});
            leaves.results.push_back(ExprResult::Pending);
            leaves.owners.push_back(owner);
            leaves.parents.emplace_back();
            leaf_parents.emplace_back();
            ConditionType type{};
            switch (expression.type) {
                case ExpressionType::EntityDestroyed:
                    type = ConditionType::EntityDestroyed;
                    rules.destroyed_entities.push_back(entity_indices.at(expression.entity));
                    break;
                case ExpressionType::EntityReachedPosition:
                    type = ConditionType::EntityReachedPosition;
                    rules.reached_entities.push_back(entity_indices.at(expression.entity));
                    rules.positions.push_back(expression.position);
                    rules.radius_squared.push_back(expression.radius * expression.radius);
                    break;
                case ExpressionType::TimeElapsed: {
                    type = ConditionType::TimeElapsed;
                    auto const ticks{expression.seconds * clock.get_tick_rate().get_hz()};
                    if (ticks >= static_cast<double>(std::numeric_limits<SimTick>::max())) {
                        return std::unexpected{
                            std::vector<std::string>{"Mission timer exceeds tick range"}};
                    }
                    rules.durations.push_back(
                        clock.duration_to_tick_period(Duration::make(expression.seconds)));
                    break;
                }
                case ExpressionType::ObjectiveResolved: {
                    type = ConditionType::ObjectiveResolved;
                    auto const observed{objectives.at(expression.objective.objective)};
                    observers[observed].push_back(leaf);
                    rules.observed_objectives.push_back(observed);
                    rules.observed_outcomes.push_back(expression.objective.outcome);
                    break;
                }
                case ExpressionType::EntityKillsReached:
                    type = ConditionType::EntityKillsReached;
                    rules.kill_sources.push_back(
                        {size(rules.kill_entities), size(expression.entities)});
                    rules.kill_targets.push_back(expression.kills);
                    for (auto const& entity : expression.entities) {
                        rules.kill_entities.push_back(entity_indices.at(entity));
                    }
                    break;
                default:
                    std::unreachable();
            }
            auto& rule_leaves{rules.leaves[static_cast<std::size_t>(type)]};
            leaves.rules.push_back(RuleId::make(type, size(rule_leaves)));
            rule_leaves.push_back(leaf);
        }
        auto const expression_count{source.expressions.size()};
        for (std::uint32_t index{}; index < expression_count; ++index) {
            auto const parent{references[index]};
            if (parent.leaf()) {
                continue;
            }
            auto children{source.expressions[index].children};
            std::ranges::sort(children);
            children.erase(std::unique(children.begin(), children.end()), children.end());
            composites.children[parent.index()] = {size(result.children), size(children)};
            composites.remaining[parent.index()] = size(children);
            for (auto const child_index : children) {
                auto const child{references[child_index]};
                result.children.push_back(child);
                (child.leaf() ? leaf_parents : composite_parents)[child.index()].push_back(
                    parent.index());
            }
        }
        for (std::size_t root{}; root < source.roots.size(); ++root) {
            if (source.roots[root] != levels::absent_expression) {
                objective.roots[root] = references[source.roots[root]];
            }
        }
        objective.leaves.count = size(leaves.results) - objective.leaves.offset;
        objective.composites.count = size(composites.results) - objective.composites.offset;
    }
    for (auto const& mapping : definition.team_results) {
        auto const owner{objectives.at(mapping.when.objective)};
        mappings[owner].push_back(size(result.team_results));
        result.team_results.push_back({mapping.team, owner, mapping.when.outcome, mapping.result});
    }
    auto flatten = [&](auto const& groups, auto& spans, auto& values) {
        auto const group_count{groups.size()};
        for (std::uint32_t index{}; index < group_count; ++index) {
            spans[index] = {size(values), size(groups[index])};
            values.insert(values.end(), groups[index].begin(), groups[index].end());
        }
    };
    flatten(composite_parents, composites.parents, result.parents);
    flatten(leaf_parents, leaves.parents, result.parents);
    for (ObjectiveId id{}; id < count; ++id) {
        auto& objective{result.objectives[id]};
        objective.dependents = {size(result.dependents), size(dependents[id])};
        result.dependents.insert(
            result.dependents.end(), dependents[id].begin(), dependents[id].end());
        objective.observers = {size(result.observers), size(observers[id])};
        result.observers.insert(result.observers.end(), observers[id].begin(), observers[id].end());
        objective.team_results = {size(result.result_indices), size(mappings[id])};
        result.result_indices.insert(
            result.result_indices.end(), mappings[id].begin(), mappings[id].end());
    }
    return result;
}
}
