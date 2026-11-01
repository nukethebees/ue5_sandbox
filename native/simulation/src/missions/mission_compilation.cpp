#include <ioj/levels/mission_validation.h>
#include <ioj/sim/missions/mission.h>
#include <ioj/sim/sim_clock.h>

#include <limits>
#include <unordered_map>
#include <utility>

namespace ioj::sim {
namespace mission_compilation_detail {
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

auto Mission::compile(levels::MissionDefinition const& definition,
                      std::span<levels::EntityId const> const entities,
                      SimClock const& clock) -> std::expected<Mission, std::vector<std::string>> {
    auto errors{levels::validate_mission(definition, entities)};
    if (!errors.empty()) {
        return std::unexpected{std::move(errors)};
    }
    auto const count{definition.objectives.size()};
    Mission result;
    result.objectives_ = Objectives{count};
    std::unordered_map<levels::ObjectiveName, ObjectiveId> objectives;
    std::unordered_map<levels::EntityId, LevelEntityIndex> entity_indices;
    auto const entity_count{entities.size()};
    for (LevelEntityIndex index{}; index < entity_count; ++index) {
        entity_indices.emplace(entities[index], index);
    }
    for (std::uint32_t index{}; index < count; ++index) {
        objectives.emplace(definition.objectives[index].id, ObjectiveId{index});
    }
    auto& composites{result.composites_};
    auto& leaves{result.leaves_};
    std::vector<std::vector<ActivationNotification>> dependents(count);
    auto size = [](auto const& values) { return static_cast<std::uint32_t>(values.size()); };

    for (std::uint32_t row{}; row < count; ++row) {
        ObjectiveId const owner{row};
        auto const& source{definition.objectives[row]};
        result.objectives_.name(owner) = source.id;
        result.objectives_.title(owner) = source.title.empty() ? source.id.value : source.title;
        result.objectives_.team(owner) = source.team;
        result.objectives_.wins_level(owner) = source.wins_level;
        result.objectives_.prerequisites(owner) =
            static_cast<ObjectivePrerequisiteCount>(source.dependencies.size());
        result.objectives_.leaves(owner).offset = size(leaves.results);
        result.objectives_.composites(owner).offset = size(composites.results);
        for (auto const& dependency : source.dependencies) {
            dependents[objectives.at(dependency.objective).index()].push_back(
                {owner, dependency.outcome});
        }
        std::vector<ExprHandle> references(source.expressions.size());
        auto const order{mission_compilation_detail::expression_order(source.expressions)};
        for (auto const index : order) {
            auto const& expression{source.expressions[index]};
            if (expression.type() == levels::ExprType::All ||
                expression.type() == levels::ExprType::Any) {
                ExprCompositeIndex const composite{size(composites.results)};
                auto const kind{expression.type() == levels::ExprType::All ? CompositeKind::All
                                                                           : CompositeKind::Any};
                references[index] = ExprHandle::make_data_composite(kind, composite);
                composites.results.push_back(ExprResult::Pending);
                composites.kinds.push_back(kind);
                auto const children{expression.children()};
                composites.child_ranges.push_back({size(composites.children), size(children)});
                for (auto const child : children) {
                    composites.children.push_back(references[child]);
                }
                continue;
            }
            ExprLeafHandle const leaf{size(leaves.results)};
            leaves.results.push_back(ExprResult::Pending);
            leaves.owners.push_back(owner);
            LeafKind kind{};
            switch (expression.type()) {
                case levels::ExprType::EntityDestroyed: {
                    auto const& condition{
                        std::get<levels::EntityDestroyedExpr>(expression.payload)};
                    kind = LeafKind::EntityDestroyed;
                    leaves.destroyed_entities.push_back(entity_indices.at(condition.entity));
                    break;
                }
                case levels::ExprType::EntityReachedPosition: {
                    auto const& condition{
                        std::get<levels::EntityReachedPositionExpr>(expression.payload)};
                    kind = LeafKind::EntityReachedPosition;
                    leaves.reached_entities.push_back(entity_indices.at(condition.entity));
                    leaves.positions.push_back(condition.position.get());
                    leaves.radius_squared.push_back(condition.radius.get() *
                                                    condition.radius.get());
                    break;
                }
                case levels::ExprType::TimeElapsed: {
                    auto const& condition{std::get<levels::TimeElapsedExpr>(expression.payload)};
                    kind = LeafKind::TimeElapsed;
                    auto const ticks{condition.seconds.get() * clock.get_tick_rate().get_hz()};
                    if (ticks >=
                        static_cast<double>(std::numeric_limits<SimTick::value_type>::max())) {
                        return std::unexpected{
                            std::vector<std::string>{"Mission timer exceeds tick range"}};
                    }
                    leaves.durations.emplace_back(
                        clock.duration_to_tick_period(Duration::make(condition.seconds.get()))
                            .get());
                    break;
                }
                case levels::ExprType::ObjectiveResolved: {
                    auto const& condition{
                        std::get<levels::ObjectiveResolvedExpr>(expression.payload)};
                    kind = LeafKind::ObjectiveResolved;
                    auto const observed{objectives.at(condition.objective.objective)};
                    leaves.observed_objectives.push_back(observed);
                    leaves.observed_outcomes.push_back(condition.objective.outcome);
                    break;
                }
                case levels::ExprType::EntityKillsReached: {
                    auto const& condition{
                        std::get<levels::EntityKillsReachedExpr>(expression.payload)};
                    kind = LeafKind::EntityKillsReached;
                    leaves.kill_sources.push_back(
                        {size(leaves.kill_entities), size(condition.entities)});
                    leaves.kill_targets.push_back(condition.kills);
                    for (auto const& entity : condition.entities) {
                        leaves.kill_entities.push_back(entity_indices.at(entity));
                    }
                    break;
                }
                default:
                    std::unreachable();
            }
            references[index] = ExprHandle::make_data_leaf(kind, leaf);
            auto& leaf_handles{leaves.leaf_handles[kind]};
            leaves.data_handles.push_back(LeafDataHandle{size(leaf_handles), kind});
            leaf_handles.push_back(leaf);
        }
        for (auto const root : EnumTraits<ObjectiveOutcome>::values) {
            if (source.roots[root] != levels::absent_expression) {
                result.objectives_.roots(owner)[root] = references[source.roots[root]];
            }
        }
        result.objectives_.leaves(owner).count =
            size(leaves.results) - result.objectives_.leaves(owner).offset;
        result.objectives_.composites(owner).count =
            size(composites.results) - result.objectives_.composites(owner).offset;
    }
    for (std::uint32_t row{}; row < count; ++row) {
        ObjectiveId const id{row};
        result.objectives_.dependents(id) = {size(result.dependents_), size(dependents[row])};
        result.dependents_.insert(
            result.dependents_.end(), dependents[row].begin(), dependents[row].end());
    }
    return result;
}
}
