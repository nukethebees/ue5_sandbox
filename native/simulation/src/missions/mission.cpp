#include <ioj/sim/missions/mission.h>

#include <ioj/frame_array.h>
#include <ioj/sim/collision/spatial_query_manager.h>
#include <ioj/sim/entities/entity_ledger.h>
#include <ioj/vector_value_math.h>
#include <ioj/vectors3f.h>

#include <algorithm>
#include <limits>

namespace ioj::sim {
namespace mission_evaluation_detail {
auto entity(MissionEvaluationContext const& inputs, std::uint32_t const slot) -> EntityId {
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
Mission::Mission(CompiledMission definition)
    : data_{std::move(definition)} {
    auto const count{data_.objectives.size()};
    inactive_.reserve(count);

    for (std::uint32_t index{}; index < count; ++index) {
        ObjectiveId const id{index};
        inactive_.push_back(id);
        data_.objectives.membership_position(id) = index;
    }
}
void Mission::begin(MissionEvaluationContext const& inputs) {
    assert(!begun_);

    begun_ = true;
    auto const count{data_.objectives.size()};

    for (std::uint32_t index{}; index < count; ++index) {
        ObjectiveId const id{index};
        if (data_.objectives.prerequisites(id) == 0) {
            activate(id, inputs.tick);
        }
    }

    settle(inputs);
}
void Mission::tick(MissionEvaluationContext const& inputs) {
    assert(begun_);

    std::span<RuleId const> const rules{active_rules_};

    for (auto const run : runs_) {
        if (run.type == ConditionType::ObjectiveResolved) {
            continue;
        }
        // NOLINTNEXTLINE(ioj-loop-view-accessor-call) -- each run selects a different rule range.
        evaluate_run(run.type, rules.subspan(run.rules.offset, run.rules.count), inputs);
    }

    settle(inputs);
}

/* **************************************** */
// Inspection
/* **************************************** */
auto Mission::take_completions() -> std::vector<TeamCompletion> {
    return std::exchange(completions_, {});
}
auto Mission::outcome(ObjectiveId const id) const -> std::optional<ObjectiveOutcome> {
    return data_.objectives.outcome(id);
}
auto Mission::team_outcome(Team const team) const -> std::optional<TeamOutcome> {
    assert(static_cast<std::size_t>(team) < team_outcomes_.size());
    return team_outcomes_[team];
}
auto Mission::result(ExprRef const expression) const -> ExprResult {
    if (!expression.is_valid()) {
        return ExprResult::False;
    }

    auto const& results{expression.leaf() ? data_.leaves.results : data_.composites.results};
    assert(expression.index() < results.size());
    return results[expression.index()];
}

/* **************************************** */
// Objective progression
/* **************************************** */
auto Mission::active(ObjectiveId const id) const -> bool {
    return data_.objectives.activation_tick(id) != std::numeric_limits<SimTick>::max() &&
           !data_.objectives.outcome(id);
}
void Mission::remove_member(std::vector<ObjectiveId>& collection, ObjectiveId const id) {
    auto const position{data_.objectives.membership_position(id)};
    assert(position < collection.size() && collection[position] == id);
    collection[position] = collection.back();
    data_.objectives.membership_position(collection.back()) = position;
    collection.pop_back();
}
void Mission::activate(ObjectiveId const id, SimTick const tick) {
    assert(!data_.objectives.outcome(id) &&
           data_.objectives.activation_tick(id) == std::numeric_limits<SimTick>::max());

    remove_member(inactive_, id);
    data_.objectives.membership_position(id) = static_cast<std::uint32_t>(active_.size());
    active_.push_back(id);
    data_.objectives.activation_tick(id) = tick;

    auto const leaves{data_.objectives.leaves(id)};
    auto const leaves_end{leaves.end()};

    for (auto leaf{leaves.start()}; leaf < leaves_end; ++leaf) {
        auto const rule{data_.leaves.rules[leaf]};
        active_rules_.push_back(rule);
        newly_active_.push_back(rule);
    }

    membership_changed_ = true;

    auto const composites{data_.objectives.composites(id)};
    auto const composites_end{composites.end()};

    for (auto node{composites.start()}; node < composites_end; ++node) {
        if (data_.composites.remaining[node] == 0) {
            resolve(ExprRef{node, false},
                    data_.composites.types[node] == ExprType::All ? ExprResult::True
                                                                  : ExprResult::False);
        }
    }

    pending_objectives_.push_back(id);
}
void Mission::complete(ObjectiveId const id, ObjectiveOutcome const outcome, SimTick const tick) {
    assert(active(id));

    remove_member(active_, id);
    data_.objectives.membership_position(id) = static_cast<std::uint32_t>(completed_.size());
    completed_.push_back(id);
    data_.objectives.outcome(id) = outcome;
    membership_changed_ = true;

    auto const dependents{data_.objectives.dependents(id)};
    auto const dependents_end{dependents.end()};

    for (auto index{dependents.start()}; index < dependents_end; ++index) {
        auto const dependency{data_.dependents[index]};
        if (dependency.outcome == outcome) {
            auto& remaining{data_.objectives.prerequisites(dependency.dependent)};
            assert(remaining > 0);
            if (--remaining == 0) {
                activate(dependency.dependent, tick);
            }
        }
    }

    auto const observers{data_.objectives.observers(id)};
    auto const observers_end{observers.end()};

    for (auto index{observers.start()}; index < observers_end; ++index) {
        auto const leaf{data_.observers[index]};
        if (active(data_.leaves.owners[leaf])) {
            auto const rule{data_.leaves.rules[leaf]};
            resolve(ExprRef{leaf, true},
                    data_.rules.observed_outcomes[rule.index()] == outcome ? ExprResult::True
                                                                           : ExprResult::False);
        }
    }

    auto const team_results{data_.objectives.team_results(id)};
    auto const team_results_end{team_results.end()};

    for (auto index{team_results.start()}; index < team_results_end; ++index) {
        auto const mapping{data_.result_indices[index]};
        if (data_.team_results[mapping].outcome == outcome) {
            candidate_results_.push_back(mapping);
        }
    }
}

/* **************************************** */
// Expression propagation
/* **************************************** */
void Mission::resolve(ExprRef const expression, ExprResult const value) {
    assert(value != ExprResult::Pending);

    auto& values{expression.leaf() ? data_.leaves.results : data_.composites.results};
    auto const index{expression.index()};
    if (values[index] != ExprResult::Pending) {
        return;
    }
    values[index] = value;
    propagation_.push_back(expression);
    pending_objectives_.push_back(expression.leaf() ? data_.leaves.owners[index]
                                                    : data_.composites.owners[index]);
    membership_changed_ = membership_changed_ || expression.leaf();
}
void Mission::resolve_rule(RuleId const rule, ExprResult const value) {
    resolve(ExprRef{rule_leaf(rule), true}, value);
}
void Mission::propagate() {
    // NOLINTNEXTLINE(ioj-loop-condition-call) -- propagation appends newly resolved parents.
    for (std::size_t index{}; index < propagation_.size(); ++index) {
        auto const child{propagation_[index]};
        auto const value{result(child)};
        auto const parents{child.leaf() ? data_.leaves.parents[child.index()]
                                        : data_.composites.parents[child.index()]};
        auto const parents_end{parents.end()};
        for (auto edge{parents.start()}; edge < parents_end; ++edge) {
            auto const parent{data_.parents[edge]};
            if (data_.composites.results[parent] != ExprResult::Pending) {
                continue;
            }
            auto const all{data_.composites.types[parent] == ExprType::All};
            if ((all && value == ExprResult::False) || (!all && value == ExprResult::True)) {
                resolve(ExprRef{parent, false}, value);
            } else {
                auto& remaining{data_.composites.remaining[parent]};
                assert(remaining > 0);
                if (--remaining == 0) {
                    resolve(ExprRef{parent, false}, all ? ExprResult::True : ExprResult::False);
                }
            }
        }
    }

    propagation_.clear();
}

/* **************************************** */
// Rule evaluation
/* **************************************** */
auto Mission::rule_leaf(RuleId const rule) const -> std::uint32_t {
    return data_.rules.leaves[rule.type()][rule.index()];
}
auto Mission::pending_rule(RuleId const rule) const -> bool {
    auto const leaf{rule_leaf(rule)};
    return active(data_.leaves.owners[leaf]) && data_.leaves.results[leaf] == ExprResult::Pending;
}
void Mission::rebuild_runs() {
    if (!membership_changed_) {
        return;
    }

    std::erase_if(active_rules_, [&](RuleId const rule) { return !pending_rule(rule); });
    std::ranges::sort(active_rules_);
    runs_.clear();
    auto const count{static_cast<std::uint32_t>(active_rules_.size())};

    for (std::uint32_t index{}; index < count; ++index) {
        auto const type{active_rules_[index].type()};
        if (runs_.empty() || runs_.back().type != type) {
            runs_.push_back({type, {index, 0}});
        }
        ++runs_.back().rules.count;
    }

    membership_changed_ = false;
}
void Mission::evaluate(std::span<RuleId const> const scheduled,
                       MissionEvaluationContext const& inputs) {
    // Newly activated rules are sorted separately; ordinary ticks use cached runs.
    auto const count{scheduled.size()};
    std::size_t first{};

    while (first < count) {
        auto const type{scheduled[first].type()};
        auto end{first + 1};
        while (end < count && scheduled[end].type() == type) {
            ++end;
        }

        // NOLINTNEXTLINE(ioj-loop-view-accessor-call) -- select the current type's rule range.
        evaluate_run(type, scheduled.subspan(first, end - first), inputs);
        first = end;
    }
}
void Mission::evaluate_run(ConditionType const type,
                           std::span<RuleId const> const run,
                           MissionEvaluationContext const& inputs) {
    switch (type) {
        case ConditionType::EntityDestroyed:
            evaluate_destroyed(run, inputs);
            break;
        case ConditionType::TimeElapsed:
            evaluate_elapsed(run, inputs);
            break;
        case ConditionType::ObjectiveResolved:
            evaluate_resolved(run);
            break;
        case ConditionType::EntityKillsReached:
            evaluate_kills(run, inputs);
            break;
        case ConditionType::EntityReachedPosition:
            evaluate_reached_position(run, inputs);
            break;
    }
}
void Mission::evaluate_destroyed(std::span<RuleId const> const run,
                                 MissionEvaluationContext const& inputs) {
    auto const& rules{data_.rules};
    auto const life_states{inputs.ledger.get_unique_entities().life_state()};

    for (auto const rule : run) {
        if (pending_rule(rule)) {
            auto const id{
                mission_evaluation_detail::entity(inputs, rules.destroyed_entities[rule.index()])};
            if (id.is_valid() &&
                mission_evaluation_detail::destroyed(id, inputs.ledger, life_states)) {
                resolve_rule(rule, ExprResult::True);
            }
        }
    }
}
void Mission::evaluate_elapsed(std::span<RuleId const> const run,
                               MissionEvaluationContext const& inputs) {
    auto const& rules{data_.rules};

    for (auto const rule : run) {
        if (pending_rule(rule)) {
            auto const activated{
                data_.objectives.activation_tick(data_.leaves.owners[rule_leaf(rule)])};
            assert(inputs.tick >= activated);
            if (inputs.tick - activated >= rules.durations[rule.index()]) {
                resolve_rule(rule, ExprResult::True);
            }
        }
    }
}
void Mission::evaluate_resolved(std::span<RuleId const> const run) {
    auto const& rules{data_.rules};

    // Resolve once on activation; subsequent changes arrive via reverse observers.
    for (auto const rule : run) {
        if (pending_rule(rule)) {
            auto const outcome{data_.objectives.outcome(rules.observed_objectives[rule.index()])};
            if (outcome) {
                resolve_rule(rule,
                             *outcome == rules.observed_outcomes[rule.index()] ? ExprResult::True
                                                                               : ExprResult::False);
            }
        }
    }
}
void Mission::evaluate_kills(std::span<RuleId const> const run,
                             MissionEvaluationContext const& inputs) {
    auto const& rules{data_.rules};

    for (auto const rule : run) {
        if (!pending_rule(rule)) {
            continue;
        }

        std::uint64_t kills{};
        auto const sources{rules.kill_sources[rule.index()]};
        auto const sources_end{sources.end()};
        for (auto index{sources.start()}; index < sources_end; ++index) {
            auto const id{mission_evaluation_detail::entity(inputs, rules.kill_entities[index])};
            if (id.is_valid()) {
                kills += inputs.ledger.get_kills(id);
            }
        }

        if (kills >= rules.kill_targets[rule.index()]) {
            resolve_rule(rule, ExprResult::True);
        }
    }
}
void Mission::evaluate_reached_position(std::span<RuleId const> const run,
                                        MissionEvaluationContext const& inputs) {
    auto const& rules{data_.rules};
    auto const life_states{inputs.ledger.get_unique_entities().life_state()};

    assert(inputs.spatial && inputs.scratch);

    FrameArray<EntityId> ids{inputs.scratch};
    FrameArray<RuleId> live_rules{inputs.scratch};
    auto const count{static_cast<std::uint32_t>(run.size())};
    ids.set_num(count);
    live_rules.set_num(count);
    std::uint32_t live_count{};

    for (auto const rule : run) {
        if (!pending_rule(rule)) {
            continue;
        }

        auto const id{
            mission_evaluation_detail::entity(inputs, rules.reached_entities[rule.index()])};
        if (!id.is_valid()) {
            continue;
        }

        if (mission_evaluation_detail::destroyed(id, inputs.ledger, life_states)) {
            resolve_rule(rule, ExprResult::False);
        } else {
            ids[live_count] = id;
            live_rules[live_count++] = rule;
        }
    }

    Vectors3f positions{*inputs.scratch};
    positions.add_uninitialised(live_count);
    auto const output{positions.get_view().resolve()};
    inputs.spatial->copy_entity_locations(
        std::span<EntityId const>{ids.data(), live_count}, output, inputs.scratch);

    for (std::uint32_t row{}; row < live_count; ++row) {
        auto const rule{live_rules[row]};
        auto const position{output[row]};
        auto const target{rules.positions[rule.index()]};
        Vector3d const position_double{position.x(), position.y(), position.z()};
        if (size_squared(position_double - target) <= rules.radius_squared[rule.index()]) {
            resolve_rule(rule, ExprResult::True);
        }
    }
}

/* **************************************** */
// Settlement
/* **************************************** */
void Mission::settle(MissionEvaluationContext const& inputs) {
    while (!newly_active_.empty() || !propagation_.empty() || !pending_objectives_.empty()) {
        if (!newly_active_.empty()) {
            auto rules{std::exchange(newly_active_, {})};
            std::ranges::sort(rules);
            evaluate(rules, inputs);
        }

        propagate();
        auto pending{std::exchange(pending_objectives_, {})};
        std::ranges::sort(pending);
        for (auto const id : pending) {
            if (!active(id)) {
                continue;
            }
            auto const& roots{data_.objectives.roots(id)};
            for (auto const root : EnumTraits<ObjectiveOutcome>::values) {
                if (result(roots[root]) == ExprResult::True) {
                    complete(id, root, inputs.tick);
                    break;
                }
            }
        }
    }

    std::ranges::sort(candidate_results_, [&](std::uint32_t const left, std::uint32_t const right) {
        auto const& a{data_.team_results[left]};
        auto const& b{data_.team_results[right]};
        if (a.team != b.team) {
            return a.team < b.team;
        }

        if (a.result != b.result) {
            return a.result == TeamOutcome::Defeat;
        }
        return left < right;
    });

    for (auto const index : candidate_results_) {
        auto const mapping{data_.team_results[index]};
        auto& outcome{team_outcomes_[mapping.team]};
        if (!outcome) {
            outcome = mapping.result;
            completions_.push_back(
                {mapping.team, mapping.result, mapping.objective, mapping.outcome, inputs.tick});
        }
    }

    candidate_results_.clear();
    rebuild_runs();
}
}
