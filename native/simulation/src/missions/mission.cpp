#include <ioj/sim/missions/mission.h>

#include <ioj/frame_array.h>
#include <ioj/levels/mission_validation.h>
#include <ioj/sim/collision/spatial_query_manager.h>
#include <ioj/sim/entities/entity_ledger.h>
#include <ioj/sim/sim_clock.h>
#include <ioj/vector_value_math.h>
#include <ioj/vectors3f.h>

#include <algorithm>
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

namespace mission_evaluation_detail {
auto entity(MissionEvaluationContext const& inputs, LevelEntityIndex const slot) -> EntityId {
    assert(slot < inputs.entities.size());
    return inputs.entities[slot];
}
auto destroyed(EntityId const id,
               EntityLedger const& ledger,
               std::span<LifeState const> const life_states) -> bool {
    assert(id.is_valid());
    auto const row{ledger.get_history_index(id)};
    assert(row < life_states.size());
    return life_states[row] != LifeState::Alive;
}
}

/* **************************************** */
// Lifecycle
/* **************************************** */
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
        result.objectives_.remaining_prerequisites(owner) =
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
                    leaves.activated_at.emplace_back();
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
        result.objectives_.add_dependents(id, dependents[row]);
    }

    return result;
}
void Mission::begin(MissionEvaluationContext const& inputs) {
    assert(!begun_);

    begun_ = true;
    auto const count{objectives_.size()};

    for (std::uint32_t index{}; index < count; ++index) {
        ObjectiveId const id{index};
        if (objectives_.remaining_prerequisites(id) == 0) {
            activate(id, inputs.tick);
        }
    }

    std::ranges::sort(leaves_.active);
    tick(inputs);
}
void Mission::tick(MissionEvaluationContext const& inputs) {
    assert(begun_);

    evaluate_leaves(inputs);
    evaluate_composites();
    evaluate_objectives(inputs);

    std::ranges::sort(leaves_.active);
}

/* **************************************** */
// Inspection
/* **************************************** */
auto Mission::take_completions() -> std::vector<TeamMissionCompletion> {
    return std::exchange(completions_, {});
}
auto Mission::take_team_completion(std::optional<Team> const team)
    -> std::optional<TeamMissionCompletion> {
    std::optional<TeamMissionCompletion> result;
    if (team) {
        auto const found{std::ranges::find(completions_, *team, &TeamMissionCompletion::team)};
        if (found != completions_.end()) {
            result = *found;
        }
    }

    completions_.clear();
    return result;
}
auto Mission::outcome(ObjectiveId const id) const -> std::optional<ObjectiveOutcome> {
    return objectives_.is_completed(id) ? root_outcome(id) : std::nullopt;
}
auto Mission::team_outcome(Team const team) const -> std::optional<MissionOutcome> {
    assert(static_cast<std::size_t>(team) < team_outcomes_.size());
    return team_outcomes_[team];
}
auto Mission::result(ExprHandle const expression) const -> ExprResult {
    if (!expression.is_valid()) {
        return ExprResult::False;
    }

    auto const leaf{expression.kind() == ExprKind::Leaf};
    auto const& results{leaf ? leaves_.results : composites_.results};
    auto const index{leaf ? expression.data_leaf_index() : expression.data_composite_index()};
    assert(index < results.size());
    return results[index];
}

/* **************************************** */
// Objective progression
/* **************************************** */
void Mission::activate(ObjectiveId const id, SimTick const tick) {
    objectives_.activate(id);

    auto const leaves{objectives_.leaves(id)};
    auto const leaves_end{leaves.end()};

    for (ExprLeafHandle leaf{leaves.start()}; leaf < leaves_end; ++leaf) {
        auto const data_handle{leaves_.data_handles[leaf]};
        if (data_handle.kind() == LeafKind::TimeElapsed) {
            leaves_.activated_at[data_handle.index()] = tick;
        }

        leaves_.active.push_back(data_handle);
    }

    auto const composites{objectives_.composites(id)};
    auto const composites_end{composites.end()};
    for (ExprCompositeIndex index{composites.start()}; index < composites_end; ++index) {
        composites_.active.push_back(index);
    }
}
void Mission::deactivate_expressions(ObjectiveId const id) {
    auto const leaves{objectives_.leaves(id)};
    std::erase_if(leaves_.active, [&](LeafDataHandle const data_handle) {
        auto const index{expression_leaf(data_handle)};
        return index >= leaves.start() && index < leaves.end();
    });

    auto const composites{objectives_.composites(id)};
    std::erase_if(composites_.active, [&](ExprCompositeIndex const index) {
        return index >= composites.start() && index < composites.end();
    });
}
void Mission::handle_completion(ObjectiveId const id,
                                ObjectiveOutcome const outcome,
                                SimTick const tick) {
    for (auto const dependency : objectives_.dependents(id)) {
        if (dependency.outcome == outcome) {
            auto& remaining{objectives_.remaining_prerequisites(dependency.dependent)};
            assert(remaining > 0);

            if (--remaining == 0) {
                activate(dependency.dependent, tick);
            }
        }
    }
}

/* **************************************** */
// Objective evaluation
/* **************************************** */
void Mission::evaluate_objectives(MissionEvaluationContext const& inputs) {
    struct Completion {
        ObjectiveId id;
        ObjectiveOutcome outcome;
    };

    FrameArray<Completion> completed{inputs.scratch};
    for (auto const id : objectives_.active()) {
        if (auto const outcome{root_outcome(id)}) {
            completed.add({id, *outcome});
        }
    }

    for (auto const [id, outcome] : completed) {
        objectives_.complete(id);
        deactivate_expressions(id);
        handle_completion(id, outcome, inputs.tick);

        if (!winning_team_ && outcome == ObjectiveOutcome::Passed &&
            objectives_.wins_level(id) == ObjectiveWinsLevel::Yes) {
            finish(id, inputs);
        }
    }
}
auto Mission::root_outcome(ObjectiveId const id) const -> std::optional<ObjectiveOutcome> {
    auto const& roots{objectives_.roots(id)};
    for (auto const outcome : EnumTraits<ObjectiveOutcome>::values) {
        if (result(roots[outcome]) == ExprResult::True) {
            return outcome;
        }
    }

    return std::nullopt;
}

/* **************************************** */
// Composite evaluation
/* **************************************** */
void Mission::evaluate_composites() {
    for (auto const index : composites_.active) {
        composites_.results[index] = evaluate_composite(index);
    }

    std::erase_if(composites_.active, [&](ExprCompositeIndex const index) {
        return composites_.results[index] != ExprResult::Pending;
    });
}
auto Mission::evaluate_composite(ExprCompositeIndex const index) const -> ExprResult {
    auto const all{composites_.kinds[index] == CompositeKind::All};
    auto const decisive{all ? ExprResult::False : ExprResult::True};
    auto value{all ? ExprResult::True : ExprResult::False};

    auto const children{composites_.child_ranges[index]};
    auto const children_end{children.end()};

    for (auto child{children.start()}; child < children_end; ++child) {
        auto const child_result{result(composites_.children[child])};
        if (child_result == decisive) {
            return decisive;
        }

        if (child_result == ExprResult::Pending) {
            value = ExprResult::Pending;
        }
    }

    return value;
}

/* **************************************** */
// Leaf evaluation
/* **************************************** */
auto Mission::expression_leaf(LeafDataHandle const data_handle) const -> ExprLeafHandle {
    return leaves_.leaf_handles[data_handle.kind()][data_handle.index()];
}
void Mission::resolve_leaf(LeafDataHandle const data_handle, ExprResult const value) {
    assert(value != ExprResult::Pending);
    leaves_.results[expression_leaf(data_handle)] = value;
}
void Mission::evaluate_leaves(MissionEvaluationContext const& inputs) {
    auto const scheduled{std::span<LeafDataHandle const>{leaves_.active}};
    assert(std::ranges::is_sorted(scheduled));

    auto const count{scheduled.size()};
    std::size_t first{};

    while (first < count) {
        auto const kind{scheduled[first].kind()};
        auto end{first + 1};
        while (end < count && scheduled[end].kind() == kind) {
            ++end;
        }

        // NOLINTNEXTLINE(ioj-loop-view-accessor-call) -- select the current leaf kind's range.
        evaluate_run(kind, scheduled.subspan(first, end - first), inputs);
        first = end;
    }

    std::erase_if(leaves_.active, [&](LeafDataHandle const data_handle) {
        return leaves_.results[expression_leaf(data_handle)] != ExprResult::Pending;
    });
}
void Mission::evaluate_run(LeafKind const kind,
                           std::span<LeafDataHandle const> const run,
                           MissionEvaluationContext const& inputs) {
    switch (kind) {
        case LeafKind::EntityDestroyed:
            evaluate_destroyed(run, inputs);
            break;
        case LeafKind::TimeElapsed:
            evaluate_elapsed(run, inputs);
            break;
        case LeafKind::ObjectiveResolved:
            evaluate_resolved(run);
            break;
        case LeafKind::EntityKillsReached:
            evaluate_kills(run, inputs);
            break;
        case LeafKind::EntityReachedPosition:
            evaluate_reached_position(run, inputs);
            break;
    }
}
void Mission::evaluate_destroyed(std::span<LeafDataHandle const> const run,
                                 MissionEvaluationContext const& inputs) {
    auto const& leaves{leaves_};
    auto const life_states{inputs.ledger.get_unique_entities().life_state()};

    for (auto const data_handle : run) {
        auto const id{mission_evaluation_detail::entity(
            inputs, leaves.destroyed_entities[data_handle.index()])};

        if (id.is_valid() && mission_evaluation_detail::destroyed(id, inputs.ledger, life_states)) {
            resolve_leaf(data_handle, ExprResult::True);
        }
    }
}
void Mission::evaluate_elapsed(std::span<LeafDataHandle const> const run,
                               MissionEvaluationContext const& inputs) {
    auto const& leaves{leaves_};

    for (auto const data_handle : run) {
        auto const activated{leaves.activated_at[data_handle.index()]};
        assert(inputs.tick >= activated);

        if (inputs.tick.value() - activated.value() >=
            leaves.durations[data_handle.index()].value()) {
            resolve_leaf(data_handle, ExprResult::True);
        }
    }
}
void Mission::evaluate_resolved(std::span<LeafDataHandle const> const run) {
    auto const& leaves{leaves_};

    for (auto const data_handle : run) {
        auto const observed_outcome{outcome(leaves.observed_objectives[data_handle.index()])};
        if (observed_outcome) {
            resolve_leaf(data_handle,
                         *observed_outcome == leaves.observed_outcomes[data_handle.index()]
                             ? ExprResult::True
                             : ExprResult::False);
        }
    }
}
void Mission::evaluate_kills(std::span<LeafDataHandle const> const run,
                             MissionEvaluationContext const& inputs) {
    auto const& leaves{leaves_};

    for (auto const data_handle : run) {
        std::uint64_t kills{};
        auto const sources{leaves.kill_sources[data_handle.index()]};
        auto const sources_end{sources.end()};
        for (auto index{sources.start()}; index < sources_end; ++index) {
            auto const id{mission_evaluation_detail::entity(inputs, leaves.kill_entities[index])};
            if (id.is_valid()) {
                kills += inputs.ledger.get_kills(id);
            }
        }

        if (kills >= leaves.kill_targets[data_handle.index()]) {
            resolve_leaf(data_handle, ExprResult::True);
        }
    }
}
void Mission::evaluate_reached_position(std::span<LeafDataHandle const> const run,
                                        MissionEvaluationContext const& inputs) {
    auto const& leaves{leaves_};
    auto const life_states{inputs.ledger.get_unique_entities().life_state()};

    assert(inputs.spatial && inputs.scratch);

    FrameArray<EntityId> ids{inputs.scratch};
    FrameArray<LeafDataHandle> live_leaves{inputs.scratch};
    auto const count{static_cast<std::uint32_t>(run.size())};
    ids.set_num(count);
    live_leaves.set_num(count);
    std::uint32_t live_count{};

    for (auto const data_handle : run) {
        auto const id{mission_evaluation_detail::entity(
            inputs, leaves.reached_entities[data_handle.index()])};
        if (!id.is_valid()) {
            continue;
        }

        if (mission_evaluation_detail::destroyed(id, inputs.ledger, life_states)) {
            resolve_leaf(data_handle, ExprResult::False);
        } else {
            ids[live_count] = id;
            live_leaves[live_count++] = data_handle;
        }
    }

    Vectors3f positions{*inputs.scratch};
    positions.add_uninitialised(live_count);
    auto const output{positions.get_view().resolve()};

    inputs.spatial->copy_entity_locations(
        std::span<EntityId const>{ids.data(), live_count}, output, inputs.scratch);

    for (std::uint32_t row{}; row < live_count; ++row) {
        auto const data_handle{live_leaves[row]};
        auto const position{output[row]};
        auto const target{leaves.positions[data_handle.index()]};
        Vector3d const position_double{position.x(), position.y(), position.z()};

        if (size_squared(position_double - target) <= leaves.radius_squared[data_handle.index()]) {
            resolve_leaf(data_handle, ExprResult::True);
        }
    }
}

/* **************************************** */
// Mission completion
/* **************************************** */
void Mission::finish(ObjectiveId const winning_objective, MissionEvaluationContext const& inputs) {
    auto const winner{objectives_.team(winning_objective)};
    winning_team_ = winner;
    team_outcomes_[winner] = MissionOutcome::Victory;
    completions_.push_back({winner,
                            MissionOutcome::Victory,
                            winning_objective,
                            ObjectiveOutcome::Passed,
                            inputs.tick});

    for (auto const team : inputs.participating_teams) {
        if (team != winner) {
            team_outcomes_[team] = MissionOutcome::Defeat;
            completions_.push_back({team,
                                    MissionOutcome::Defeat,
                                    winning_objective,
                                    ObjectiveOutcome::Passed,
                                    inputs.tick});
        }
    }
}
}
