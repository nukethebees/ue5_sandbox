#include <ioj/sim/missions/mission.h>

#include <ioj/frame_array.h>
#include <ioj/sim/collision/spatial_query_manager.h>
#include <ioj/sim/entities/entity_ledger.h>
#include <ioj/vector_value_math.h>
#include <ioj/vectors3f.h>

#include <algorithm>

namespace ioj::sim {
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
void Mission::begin(MissionEvaluationContext const& inputs) {
    assert(!begun_);

    begun_ = true;
    auto const count{objectives_.size()};

    for (std::uint32_t index{}; index < count; ++index) {
        ObjectiveId const id{index};
        if (objectives_.prerequisites(id) == 0) {
            activate(id, inputs.tick);
        }
    }

    std::ranges::sort(leaves_.active);
    tick(inputs);
}
void Mission::tick(MissionEvaluationContext const& inputs) {
    assert(begun_);

    evaluate_leaves(inputs);

    auto const first_completed{
        static_cast<ObjectiveMembershipIndex>(objectives_.completed().size())};
    evaluate_objectives();

    auto const completed{objectives_.completed()};
    auto const completed_count{completed.size()};
    for (auto position{first_completed}; position < completed_count; ++position) {
        auto const id{completed[position]};
        objectives_.commit_completion(position);
        handle_completion(id, inputs.tick);
        if (!winning_team_ && objectives_.outcome(id) == ObjectiveOutcome::Passed &&
            objectives_.wins_level(id) == ObjectiveWinsLevel::Yes) {
            finish(id, inputs);
        }
    }

    std::erase_if(leaves_.active,
                  [&](LeafDataHandle const data_handle) { return !pending_leaf(data_handle); });
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
    return objectives_.outcome(id);
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
    objectives_.activate(id, tick);

    auto const leaves{objectives_.leaves(id)};
    auto const leaves_end{leaves.end()};

    for (ExprLeafHandle leaf{leaves.start()}; leaf < leaves_end; ++leaf) {
        auto const data_handle{leaves_.data_handles[leaf]};
        leaves_.active.push_back(data_handle);
    }
}
void Mission::handle_completion(ObjectiveId const id, SimTick const tick) {
    auto const outcome{*objectives_.outcome(id)};

    auto const dependents{objectives_.dependents(id)};
    auto const dependents_end{dependents.end()};

    for (auto index{dependents.start()}; index < dependents_end; ++index) {
        auto const dependency{dependents_[index]};
        if (dependency.outcome == outcome) {
            auto& remaining{objectives_.prerequisites(dependency.dependent)};
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
void Mission::evaluate_objectives() {
    for (auto const id : objectives_.active()) {
        auto const composites{objectives_.composites(id)};
        auto const composites_end{composites.end()};
        for (ExprCompositeIndex index{composites.start()}; index < composites_end; ++index) {
            auto& value{composites_.results[index]};
            if (value == ExprResult::Pending) {
                value = evaluate_composite(index);
            }
        }

        auto const& roots{objectives_.roots(id)};
        for (auto const outcome : EnumTraits<ObjectiveOutcome>::values) {
            if (result(roots[outcome]) == ExprResult::True) {
                objectives_.resolve(id, outcome);
                break;
            }
        }
    }
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
auto Mission::pending_leaf(LeafDataHandle const data_handle) const -> bool {
    auto const leaf{expression_leaf(data_handle)};
    return objectives_.is_active(leaves_.owners[leaf]) &&
           leaves_.results[leaf] == ExprResult::Pending;
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
        if (pending_leaf(data_handle)) {
            auto const id{mission_evaluation_detail::entity(
                inputs, leaves.destroyed_entities[data_handle.index()])};
            if (id.is_valid() &&
                mission_evaluation_detail::destroyed(id, inputs.ledger, life_states)) {
                resolve_leaf(data_handle, ExprResult::True);
            }
        }
    }
}
void Mission::evaluate_elapsed(std::span<LeafDataHandle const> const run,
                               MissionEvaluationContext const& inputs) {
    auto const& leaves{leaves_};

    for (auto const data_handle : run) {
        if (pending_leaf(data_handle)) {
            auto const activated{
                objectives_.activation_tick(leaves.owners[expression_leaf(data_handle)])};
            assert(inputs.tick >= activated);
            if (inputs.tick.value() - activated.value() >=
                leaves.durations[data_handle.index()].value()) {
                resolve_leaf(data_handle, ExprResult::True);
            }
        }
    }
}
void Mission::evaluate_resolved(std::span<LeafDataHandle const> const run) {
    auto const& leaves{leaves_};

    for (auto const data_handle : run) {
        if (pending_leaf(data_handle)) {
            auto const outcome{
                objectives_.outcome(leaves.observed_objectives[data_handle.index()])};
            if (outcome) {
                resolve_leaf(data_handle,
                             *outcome == leaves.observed_outcomes[data_handle.index()]
                                 ? ExprResult::True
                                 : ExprResult::False);
            }
        }
    }
}
void Mission::evaluate_kills(std::span<LeafDataHandle const> const run,
                             MissionEvaluationContext const& inputs) {
    auto const& leaves{leaves_};

    for (auto const data_handle : run) {
        if (!pending_leaf(data_handle)) {
            continue;
        }

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
        if (!pending_leaf(data_handle)) {
            continue;
        }

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
