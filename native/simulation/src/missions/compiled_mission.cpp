#include <ioj/sim/missions/compiled_mission.h>

#include <ioj/levels/mission_validation.h>
#include <ioj/sim/sim_clock.h>

#include <algorithm>
#include <limits>
#include <unordered_map>

namespace ioj::sim {
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
    std::vector<std::vector<std::uint32_t>> composite_parents;
    std::vector<std::vector<std::uint32_t>> leaf_parents;
    std::vector<std::vector<ActivationNotification>> dependents(count);
    std::vector<std::vector<std::uint32_t>> observers(count);
    std::vector<std::vector<TeamResultIndex>> mappings(count);
    auto size = [](auto const& values) { return static_cast<std::uint32_t>(values.size()); };

    for (std::uint32_t row{}; row < count; ++row) {
        ObjectiveId const owner{row};
        auto const& source{definition.objectives[row]};
        result.objectives.name(owner) = source.id;
        result.objectives.title(owner) = source.title.empty() ? source.id.value : source.title;
        result.objectives.team(owner) = source.team;
        result.objectives.prerequisites(owner) = size(source.dependencies);
        result.objectives.leaves(owner).offset = size(leaves.results);
        result.objectives.composites(owner).offset = size(composites.results);
        for (auto const& dependency : source.dependencies) {
            dependents[objectives.at(dependency.objective).index()].push_back(
                {owner, dependency.outcome});
        }
        std::vector<ExprRef> references;
        references.reserve(source.expressions.size());
        for (auto const& expression : source.expressions) {
            if (expression.type() == ExprType::All || expression.type() == ExprType::Any) {
                references.emplace_back(size(composites.results), false);
                composites.results.push_back(ExprResult::Pending);
                composites.types.push_back(expression.type());
                composites.owners.push_back(owner);
                composites.parents.emplace_back();
                composites.children.emplace_back();
                composites.remaining.push_back(0);
                composite_parents.emplace_back();
                continue;
            }
            auto const leaf{size(leaves.results)};
            references.emplace_back(leaf, true);
            leaves.results.push_back(ExprResult::Pending);
            leaves.owners.push_back(owner);
            leaves.parents.emplace_back();
            leaf_parents.emplace_back();
            ConditionType type{};
            switch (expression.type()) {
                case ExprType::EntityDestroyed: {
                    auto const& condition{
                        std::get<levels::EntityDestroyedExpr>(expression.payload)};
                    type = ConditionType::EntityDestroyed;
                    rules.destroyed_entities.push_back(entity_indices.at(condition.entity));
                    break;
                }
                case ExprType::EntityReachedPosition: {
                    auto const& condition{
                        std::get<levels::EntityReachedPositionExpr>(expression.payload)};
                    type = ConditionType::EntityReachedPosition;
                    rules.reached_entities.push_back(entity_indices.at(condition.entity));
                    rules.positions.push_back(condition.position.get());
                    rules.radius_squared.push_back(condition.radius.get() * condition.radius.get());
                    break;
                }
                case ExprType::TimeElapsed: {
                    auto const& condition{std::get<levels::TimeElapsedExpr>(expression.payload)};
                    type = ConditionType::TimeElapsed;
                    auto const ticks{condition.seconds.get() * clock.get_tick_rate().get_hz()};
                    if (ticks >= static_cast<double>(std::numeric_limits<SimTick>::max())) {
                        return std::unexpected{
                            std::vector<std::string>{"Mission timer exceeds tick range"}};
                    }
                    rules.durations.push_back(
                        clock.duration_to_tick_period(Duration::make(condition.seconds.get())));
                    break;
                }
                case ExprType::ObjectiveResolved: {
                    auto const& condition{
                        std::get<levels::ObjectiveResolvedExpr>(expression.payload)};
                    type = ConditionType::ObjectiveResolved;
                    auto const observed{objectives.at(condition.objective.objective)};
                    observers[observed.index()].push_back(leaf);
                    rules.observed_objectives.push_back(observed);
                    rules.observed_outcomes.push_back(condition.objective.outcome);
                    break;
                }
                case ExprType::EntityKillsReached: {
                    auto const& condition{
                        std::get<levels::EntityKillsReachedExpr>(expression.payload)};
                    type = ConditionType::EntityKillsReached;
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
        auto const expression_count{source.expressions.size()};
        for (std::uint32_t index{}; index < expression_count; ++index) {
            auto const parent{references[index]};
            if (parent.leaf()) {
                continue;
            }
            // NOLINTNEXTLINE(ioj-loop-view-accessor-call) -- children belong to the current node.
            auto const source_children{source.expressions[index].children()};
            std::vector<levels::ExprIndex> children{source_children.begin(), source_children.end()};
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
    for (auto const& mapping : definition.team_results) {
        auto const owner{objectives.at(mapping.when.objective)};
        mappings[owner.index()].push_back(size(result.team_results));
        result.team_results.push_back({owner, mapping.team, mapping.when.outcome, mapping.result});
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
    for (std::uint32_t row{}; row < count; ++row) {
        ObjectiveId const id{row};
        result.objectives.dependents(id) = {size(result.dependents), size(dependents[row])};
        result.dependents.insert(
            result.dependents.end(), dependents[row].begin(), dependents[row].end());
        result.objectives.observers(id) = {size(result.observers), size(observers[row])};
        result.observers.insert(
            result.observers.end(), observers[row].begin(), observers[row].end());
        result.objectives.team_results(id) = {size(result.result_indices), size(mappings[row])};
        result.result_indices.insert(
            result.result_indices.end(), mappings[row].begin(), mappings[row].end());
    }
    return result;
}
}
